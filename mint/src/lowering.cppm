export module mint: lowering;

import std;
import xxas;

import :lexer;
import :ir;
import :parser;
import :arch;
import :operand;
import :expression;
import :traits;
import :semantics;
import :assembler;
import :binary;

/*** **
 **
 **  module:   mint: lowering
 **  purpose:  Lowers IR programs into binary output through the assembler.
 **            Bridges the Parser's ir::Program to the Assembler by performing
 **            multi-pass symbol resolution, architecture-specific instruction
 **            encoding, data emission, and section management.
 **
 **  pipeline: Lexer -> Parser -> [Lowering] -> Assembler -> Binary
 **
 *** **/

namespace mint
{
    namespace lowering
    {
        // Symbol resolver: maps symbol names to resolved virtual addresses.
        // Passed to the encoder during instruction encoding so that operand
        // expressions referencing labels or data can be resolved to concrete values.
        export using SymbolResolver = std::function<std::optional<std::uint64_t>(std::string_view)>;

        // Encoder concept: user-defined architectures must satisfy this contract
        // to participate in the lowering pipeline.
        //
        //   encode(insn, resolver)  â†’ encoded byte sequence, or nullopt on failure.
        //   estimate_size(insn)     â†’ estimated byte count for layout pre-pass.
        //
        export template<class E>
        concept Encoder = requires(E encoder, const ir::Instruction& insn, const SymbolResolver& resolver)
        {
            { encoder.encode(insn, resolver)  } -> std::same_as<std::optional<std::vector<std::byte>>>;
            { encoder.estimate_size(insn)     } -> std::convertible_to<std::size_t>;
        };

        // Diagnostic entry for lowering issues.
        export struct Diagnostic
        {
            enum class Level: std::uint8_t
            {
                Warning,
                Error,
            };

            Level            level;
            std::size_t      line;
            std::string      message;

            constexpr Diagnostic(Level level, std::size_t line, std::string message)
                : level{ level }, line{ line }, message{ std::move(message) } {};
        };

        // Lowering error classification.
        export enum class Err: std::uint8_t
        {
            UnresolvedSymbol,
            EncodingFailed,
            DataEmission,
            EntryPoint,
            Build,
        };

        // Lowering result type.
        export using Result = xxas::Result<Binary, Err>;

        // Lowering statistics for inspection.
        export struct Stats
        {
            std::size_t instructions_emitted = 0;
            std::size_t labels_resolved      = 0;
            std::size_t data_directives      = 0;
            std::size_t total_code_bytes     = 0;
            std::size_t total_data_bytes     = 0;
        };
    };

    /***
     **  Lowering: IR Program Assembler Binary.
     **
     **  Template parameters:
     **    `arch` Architecture descriptor (compile-time constant reference).
     **    `Enc`  Encoder type satisfying lowering::Encoder.
     **
     **  The lowering process runs in two phases:
     **    1. Data pass:  emit all ir::Data directives, assigning virtual addresses.
     **    2. Code pass:  encode all ir::Instructions through the Encoder, attaching
     **                   labels as assembler symbols to the first instruction they precede.
     **
     **  Symbol resolution is available to the encoder during the code pass so that
     **  expressions referencing labels and data can be resolved to addresses.
     ***/
    export template<const auto& arch, lowering::Encoder Enc> struct Lowering
    {
        using SymbolMap   = std::unordered_map<std::string, std::uint64_t>;
        using Diagnostics = std::vector<lowering::Diagnostic>;

        Assembler       assembler;
        Enc             encoder;
        SymbolMap       symbols;
        Diagnostics     diagnostics;
        lowering::Stats stats;

        constexpr Lowering(Enc encoder)
            : encoder{ std::move(encoder) } {};

        // Build a symbol resolver that queries both the local symbol table
        // and the assembler's symbol table.
        auto make_resolver() const
            -> lowering::SymbolResolver
        {
            return [this](std::string_view name)
                -> std::optional<std::uint64_t>
            {
                if(auto it = this->symbols.find(std::string{ name }); it != this->symbols.end())
                {
                    return it->second;
                };

                return this->assembler.get_symbol_address(std::string{ name });
            };
        };

        // Resolve an expression to a concrete value using the current symbol table.
        auto resolve_expression(const Expression& expr) const
            -> std::optional<std::uint64_t>
        {
            return expr.template evaluate<std::uint64_t>(
                [this](std::string_view name)
                    -> std::optional<std::uint64_t>
                {
                    if(auto it = this->symbols.find(std::string{ name }); it != this->symbols.end())
                    {
                        return it->second;
                    };
                    return this->assembler.get_symbol_address(std::string{ name });
                }
            );
        };

        // Record a diagnostic.
        auto diagnose(lowering::Diagnostic::Level level, std::size_t line, std::string message)
            -> void
        {
            this->diagnostics.emplace_back(level, line, std::move(message));
        };

        // Check if any error-level diagnostics have been recorded.
        auto has_errors() const
            -> bool
        {
            return std::ranges::any_of(this->diagnostics, [](const auto& d)
            {
                return d.level == lowering::Diagnostic::Level::Error;
            });
        };

        // Pre-scan to collect symbol estimates from labels and data.
        // Builds an initial offset map so that the code pass can resolve
        // forward references to labels appearing later in the program.
        auto prescan_symbols(const ir::Program& program)
            -> void
        {
            std::uint64_t code_estimate = 0;

            for(const auto& node: program.nodes)
            {
                std::visit(xxas::meta::Overloads
                {
                    [&](const ir::Label& label)
                    {
                        // Record the label at the current estimated code offset.
                        // Real virtual addresses are assigned during emit_code;
                        // this estimate lets the encoder resolve forward refs.
                        this->symbols[std::string{ label.name }] = code_estimate;
                    },
                    [&](const ir::Instruction& insn)
                    {
                        code_estimate += this->encoder.estimate_size(insn);
                    },
                    [&](const ir::Data&)
                    {
                        // Data addresses are assigned during emit_data.
                    },
                }, node);
            };
        };

        // Emit all data directives into the assembler's data section.
        auto emit_data(const ir::Program& program)
            -> bool
        {
            bool ok = true;

            for(const auto& node: program.nodes)
            {
                const auto* data = std::get_if<ir::Data>(&node);
                if(!data)
                {
                    continue;
                };

                const auto byte_size = Traits{data->size}.size();
                std::vector<std::byte> bytes;
                bytes.reserve(byte_size * data->values.size());

                for(const auto& expr: data->values)
                {
                    // Try full resolution first, then constant-only fallback.
                    auto value = this->resolve_expression(expr);
                    if(!value)
                    {
                        value = expr.template evaluate<std::uint64_t>();
                    };

                    if(!value)
                    {
                        this->diagnose(
                            lowering::Diagnostic::Level::Error,
                            data->line,
                            std::format("unresolved data value for '{}'", data->label)
                        );
                        ok = false;
                        continue;
                    };

                    // Encode in little-endian byte order.
                    for(std::size_t i = 0; i < byte_size; ++i)
                    {
                        bytes.push_back(static_cast<std::byte>((*value >> (i * 8)) & 0xFF));
                    };
                };

                if(!ok)
                {
                    continue;
                };

                // Insert into assembler data section with label.
                auto label_str = std::string{ data->label };
                const auto vaddr = this->assembler.add_data(
                    std::span<const std::byte>{ bytes.data(), bytes.size() },
                    std::optional<std::string>{ label_str }
                );

                // Update symbol to the real virtual address.
                this->symbols[label_str] = vaddr;
                this->stats.data_directives++;
                this->stats.total_data_bytes += bytes.size();
            };

            return ok;
        };

        // Emit all instructions into the assembler's code section.
        auto emit_code(const ir::Program& program)
            -> bool
        {
            auto resolver = this->make_resolver();
            std::optional<std::string> pending_label;
            bool ok = true;

            for(const auto& node: program.nodes)
            {
                std::visit(xxas::meta::Overloads
                {
                    [&](const ir::Label& label)
                    {
                        // Queue this label to attach to the next instruction.
                        pending_label = std::string{ label.name };
                    },
                    [&](const ir::Instruction& insn)
                    {
                        // Encode the instruction through the architecture encoder.
                        auto encoded = this->encoder.encode(insn, resolver);
                        if(!encoded)
                        {
                            this->diagnose(
                                lowering::Diagnostic::Level::Error,
                                insn.line,
                                std::format("failed to encode instruction '{}'", insn.name)
                            );
                            ok = false;
                            return;
                        };

                        // Emit into assembler code section.
                        const auto vaddr = this->assembler.add_code(
                            std::span<const std::byte>{ encoded->data(), encoded->size() },
                            pending_label
                        );

                        // Update symbol address if a label was attached.
                        if(pending_label)
                        {
                            this->symbols[*pending_label] = vaddr;
                            this->stats.labels_resolved++;
                            pending_label.reset();
                        };

                        this->stats.instructions_emitted++;
                        this->stats.total_code_bytes += encoded->size();
                    },
                    [](const ir::Data&)
                    {
                        // Already emitted in the data pass.
                    },
                }, node);
            };

            return ok;
        };

        // Execute the full lowering pipeline for a program.
        auto lower(const ir::Program& program)
            -> lowering::Result
        {
            // Pre-scan: build initial symbol estimates for forward references.
            this->prescan_symbols(program);

            // Data pass: emit data directives (assigns real virtual addresses).
            if(!this->emit_data(program))
            {
                return this->collect_errors(lowering::Err::DataEmission);
            };

            // Code pass: encode and emit instructions.
            if(!this->emit_code(program))
            {
                return this->collect_errors(lowering::Err::EncodingFailed);
            };

            // Set the entry point.
            if(!program.entry_point.empty())
            {
                auto entry_str = std::string{ program.entry_point };

                if(auto it = this->symbols.find(entry_str); it != this->symbols.end())
                {
                    this->assembler.set_entry_point(it->second);
                }
                else
                {
                    this->assembler.set_entry_point(entry_str);
                };
            };

            // Build the final binary.
            auto result = this->assembler.build();
            if(!result.has_value())
            {
                return xxas::error(lowering::Err::Build, std::format("assembler: {}", result.error().message));
            };

            return std::move(*result);
        };

        // Lower an IR program with a given encoder.
        static auto from(const ir::Program& program, Enc encoder)
            -> lowering::Result
        {
            Lowering ctx{ std::move(encoder) };
            return ctx.lower(program);
        };

        template<const auto& rules> static auto compile(std::string_view source, Enc encoder)
            -> lowering::Result
        {
            const auto tokens  = lexer::Lexer<arch, rules>::from(source);
            const auto program = Parser<arch>::from(tokens);

            return Lowering::from(program, std::move(encoder));
        };

        // Full pipeline with explicit entry point.
        template<const auto& rules> static auto compile(std::string_view source, Enc encoder, std::string_view entry_point)
            -> lowering::Result
        {
            const auto tokens = lexer::Lexer<arch, rules>::from(source);
            auto program      = Parser<arch>::from(tokens);
            program.entry_point = entry_point;

            return Lowering::from(program, std::move(encoder));
        };

    private:

        // Collect error diagnostics into a single error result.
        auto collect_errors(lowering::Err kind) const
            -> lowering::Result
        {
            std::string msg;

            for(const auto& diag: this->diagnostics)
            {
                if(diag.level == lowering::Diagnostic::Level::Error)
                {
                    if(!msg.empty())
                    {
                        msg += "; ";
                    };
                    msg += std::format("line {}: {}", diag.line, diag.message);
                };
            };

            return xxas::error(std::move(kind), std::move(msg));
        };
    };
};
