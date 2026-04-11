export module mint: executor;

import std;
import xxas;

import :arch;
import :traits;
import :memory;
import :stackframe;
import :lifter;

/*** **
 **
 **  module:   mint: executor
 **  purpose:  Executes a lifted program against a compile-time architecture.
 **            Flat register file indexed by CMap ordinal, zero-allocation
 **            fetch-decode-execute loop with O(1) CMap-dispatched handlers.
 **
 **  pipeline: lift::Program -> [Executor] -> exit code
 **
 *** **/

namespace mint
{
    namespace exec
    {   // Compile-time register file dimensions derived from the architecture.
        template<const auto& arch> struct RegInfo
        {   // Total keyword slot count (includes non-register keywords).
            // Non-register slots are unused but the waste is negligible
            // and avoids a translation layer from CMap ordinal to register index.
            static constexpr std::size_t Count = arch.keywords.entries.size();

            // Largest register width in bytes across all keyword entries.
            static consteval auto max_size()
                -> std::size_t
            {
                std::size_t max_sz = 1;

                for(const auto& entry: arch.keywords.entries)
                {
                    const auto src = static_cast<traits::Source>(
                        entry.second.bits & std::to_underlying(traits::Source::Mask)
                    );

                    if(src == traits::Source::Register)
                    {
                        const auto sz = entry.second.size();
                        if(sz > max_sz) max_sz = sz;
                    };
                };

                return max_sz;
            };

            static constexpr std::size_t MaxBytes = max_size();
        };

        // Properly aligned register slot.
        // Each slot is aligned to its byte width so typed access
        // via reinterpret_cast is well-defined.
        template<std::size_t MaxBytes> struct alignas(MaxBytes) RegSlot
        {
            std::array<std::byte, MaxBytes> bytes{};

            template<class T> constexpr auto as()
                -> T&
            {
                return *reinterpret_cast<T*>(this->bytes.data());
            };

            template<class T> constexpr auto as() const
                -> const T&
            {
                return *reinterpret_cast<const T*>(this->bytes.data());
            };

            // Zero the slot.
            constexpr auto clear()
                -> void
            {
                this->bytes.fill(std::byte{ 0 });
            };
        };

        // Forward declare State for handler signature.
        export template<const auto& arch> struct State;

        // Handler function pointer type.
        // All instruction handlers share this uniform signature.
        export template<const auto& arch> using HandlerFn = void(*)(State<arch>&, std::span<const lift::Operand>);

        // Execution state: flat register file, instruction pointer, control flags.
        // Passed by reference to every handler invocation.
        template<const auto& arch> struct State
        {
            using Info = RegInfo<arch>;
            using Slot = RegSlot<Info::MaxBytes>;

            static constexpr auto NumSlots = Info::Count;

            // Flat register file. Indexed by CMap construction-order ordinal.
            // Cache-line aligned for optimal access patterns.
            alignas(64) std::array<Slot, NumSlots> regs{};

            // Instruction pointer (index into lift::Program::code).
            std::size_t ip = 0;

            // Control flags.
            bool         halted    = false;
            std::int64_t exit_code = 0;

            // Environment pointers. Bound during execution setup.
            const lift::Program* program = nullptr;
            Memory*              memory  = nullptr;
            StackFrame*          stack   = nullptr;

            // Typed register read/write by CMap ordinal.
            template<class T> constexpr auto reg(std::size_t ordinal)
                -> T&
            {
                return this->regs[ordinal].template as<T>();
            };

            template<class T> constexpr auto reg(std::size_t ordinal) const
                -> const T&
            {
                return this->regs[ordinal].template as<T>();
            };

            // Raw byte access to a register slot.
            constexpr auto reg_bytes(std::size_t ordinal)
                -> std::span<std::byte, Info::MaxBytes>
            {
                return std::span<std::byte, Info::MaxBytes>{ this->regs[ordinal].bytes };
            };

            constexpr auto reg_bytes(std::size_t ordinal) const
                -> std::span<const std::byte, Info::MaxBytes>
            {
                return std::span<const std::byte, Info::MaxBytes>{ this->regs[ordinal].bytes };
            };

            // Resolve a lift::Operand to its effective value.
            // Reg operands dereference the register file; Imm/Addr pass through.
            template<class T = std::uint64_t> constexpr auto resolve(const lift::Operand& op) const
                -> T
            {
                if(op.kind == lift::Kind::Reg)
                {
                    return this->reg<T>(op.value);
                };

                return static_cast<T>(op.value);
            };

            // Jump to a virtual address. Resolves through the program's code map.
            constexpr auto jump(std::uint64_t vaddr)
                -> bool
            {
                auto idx = this->program->resolve_vaddr(vaddr);
                if(!idx) return false;

                this->ip = *idx;
                return true;
            };

            // Jump to an instruction index directly.
            constexpr auto jump_to(std::size_t index)
                -> void
            {
                this->ip = index;
            };

            // Halt execution with an exit code.
            constexpr auto halt(std::int64_t code = 0)
                -> void
            {
                this->exit_code = code;
                this->halted    = true;
            };
        };

        // Executor error classification.
        export enum class Err: std::uint8_t
        {
            UnknownOpcode,
            OutOfBounds,
            Runtime,
        };

        export using Result = xxas::Result<std::int64_t, Err>;

        // Bridges generic lambdas stored in arch::Insns into concrete
        // HandlerFn<arch> function pointers. Each lambda uses the signature
        // (auto& state, auto ops) for full execution state access while
        // benefiting from the Insns CMap's O(1) constexpr lookup.

        // Default-constructs a stateless Lambda, forwards (state, ops).
        export template<const auto& arch, typename Lambda>
        void handler_fn(State<arch>& state, std::span<const lift::Operand> ops)
        {
            Lambda{}(state, ops);
        };

        // Wrap an inline lambda into HandlerFn<arch>.
        export template<const auto& arch>
        constexpr auto make_handler(auto lambda) -> HandlerFn<arch>
        {
            return &handler_fn<arch, decltype(lambda)>;
        };

        // Lookup instruction in arch.insns by mnemonic; return its handler.
        export template<const auto& arch>
        consteval auto handler_of(std::string_view name) -> HandlerFn<arch>
        {
            for(std::size_t i = 0; i < arch.insns.entries.size(); ++i)
            {
                if(arch.insns.entries[i].first == name)
                {
                    return std::visit([](auto&& lambda) -> HandlerFn<arch>
                    {
                        using Lambda = std::remove_cvref_t<decltype(lambda)>;
                        return &handler_fn<arch, Lambda>;
                    }, arch.insns.entries[i].second);
                };
            };

            throw "handler_of: instruction not found in architecture";
        };

        // Return the handler for arch.insns entry at compile-time index I.
        template<const auto& arch, std::size_t I>
        consteval auto handler_at() -> HandlerFn<arch>
        {
            return std::visit([](auto&& lambda) -> HandlerFn<arch>
            {
                using Lambda = std::remove_cvref_t<decltype(lambda)>;
                return &handler_fn<arch, Lambda>;
            }, arch.insns.entries[I].second);
        };

        // Build the full dispatch CMap from arch.insns.
        // Each entry's index becomes its opcode.
        template<const auto& arch, std::size_t... Is>
        consteval auto build_dispatch(std::index_sequence<Is...>)
        {
            return xxas::CMap
            {
                std::pair{ static_cast<std::uint16_t>(Is), handler_at<arch, Is>() }...
            };
        };

        // Auto-generate a dispatch CMap from arch.insns.
        // Opcodes are the CMap entry indices. Use insn_opcode<arch>()
        // in encoders to produce matching opcodes.
        export template<const auto& arch>
        consteval auto insn_dispatch()
        {
            return build_dispatch<arch>(
                std::make_index_sequence<arch.insns.entries.size()>{});
        };
    };

    // Resolve an instruction mnemonic to its opcode (entry index in arch.insns).
    // O(1) CMap lookup. Use in encoders to produce opcodes that match
    // the dispatch table generated by exec::insn_dispatch<arch>().
    export template<const auto& arch> constexpr auto insn_opcode(std::string_view name)
        -> std::optional<std::uint16_t>
    {
        auto it = arch.insns.find(name);
        if(it == arch.insns.end()) return std::nullopt;

        return static_cast<std::uint16_t>(
            std::distance(arch.insns.begin(), it));
    };

    // Resolve a keyword name to its CMap construction-order ordinal.
    // Use this in encoders/decoders to produce register indices that
    // directly index the executor's flat register file.
    export template<const auto& arch> constexpr auto keyword_ordinal(std::string_view name)
        -> std::optional<std::size_t>
    {
        auto it = arch.keywords.find(name);
        if(it == arch.keywords.end()) return std::nullopt;

        return static_cast<std::size_t>(std::distance(arch.keywords.begin(), it));
    };

    /***
     **  Executor: runs a lifted program to completion.
     **
     **  Template parameters:
     **    `arch`     — Architecture descriptor (compile-time constant reference).
     **    `dispatch` — CMap from opcode (uint16_t) to HandlerFn (compile-time constant reference).
     **
     **  The dispatch table is a CMap providing O(1) lookup from numeric opcode
     **  to handler function pointer. All handlers share the uniform signature:
     **
     **      void(State<arch>&, std::span<const lift::Operand>)
     **
     **  The execution loop is allocation-free: instruction fetch is a flat array
     **  index, operand access is a span into the pre-built pool, and dispatch
     **  is a single CMap probe followed by a function pointer call.
     ***/
    export template<const auto& arch, const auto& dispatch> struct Executor
    {
        using State  = exec::State<arch>;

        // Execute a lifted program from its declared entry point.
        static auto run(const lift::Program& program)
            -> exec::Result
        {
            State state{};
            state.program = &program;
            state.ip      = program.entry_ip;

            return execute(state, program);
        };

        // Execute with pre-configured memory and stack.
        static auto run(const lift::Program& program, Memory& memory, StackFrame& stack)
            -> exec::Result
        {
            State state{};
            state.program = &program;
            state.ip      = program.entry_ip;
            state.memory  = &memory;
            state.stack   = &stack;

            return execute(state, program);
        };

        // Execute with an externally managed state (for coroutine-style stepping).
        static auto run(State& state, const lift::Program& program)
            -> exec::Result
        {
            state.program = &program;
            return execute(state, program);
        };

    private:

        // Core fetch-decode-execute loop. Zero allocations.
        static auto execute(State& state, const lift::Program& program)
            -> exec::Result
        {
            const auto code_size = program.code.size();

            while(!state.halted) [[likely]]
            {
                if(state.ip >= code_size) [[unlikely]]
                {
                    // Ran off the end of the program. Normal termination.
                    break;
                };

                // Fetch: flat array index.
                const auto& insn = program.code[state.ip];

                // Decode: span into pre-built operand pool.
                const auto ops = std::span
                {
                    program.pool.data() + insn.operand_offset,
                    insn.operand_count
                };

                // Dispatch: O(1) CMap probe.
                auto it = dispatch.find(insn.opcode);

                if(it == dispatch.end()) [[unlikely]]
                {
                    return xxas::error(exec::Err::UnknownOpcode,
                        std::format("unknown opcode {} at ip {}", insn.opcode, state.ip));
                };

                // Capture ip before handler invocation.
                const auto prev_ip = state.ip;

                // Execute: function pointer call.
                it->second(state, ops);

                // Auto-advance if the handler did not modify ip.
                if(state.ip == prev_ip)
                {
                    state.ip++;
                };
            };

            return state.exit_code;
        };
    };
};
