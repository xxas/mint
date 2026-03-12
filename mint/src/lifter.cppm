export module mint: lifter;

import std;
import xxas;

import :binary;
import :arch;
import :traits;

/*** **
 **
 **  module:   mint: lifter
 **  purpose:  Lifts a serialized Binary into a flat, execution-ready program
 **            representation. Decodes the code section into contiguous instruction
 **            and operand arrays with zero per-instruction heap allocations.
 **
 **  pipeline: Binary -> [Lifter] -> lift::Program -> Executor
 **
 *** **/

namespace mint
{
    namespace lift
    {
        // Maximum operands per instruction.
        // Covers all reasonable ISAs without dynamic allocation.
        constexpr inline std::size_t MaxOperands = 4;

        // Operand sourcing classification.
        export enum class Kind: std::uint8_t
        {
            Reg,    // Register file ordinal.
            Imm,    // Immediate constant value.
            Addr,   // Virtual address (label, data pointer).
        };

        // Decoded operand: tagged value, trivially copyable.
        export struct Operand
        {
            Kind          kind;
            std::uint64_t value;
        };

        // Decoded instruction header stored in the code array.
        // Operands are referenced by offset into the shared pool.
        export struct Insn
        {
            std::uint16_t opcode;
            std::uint8_t  operand_count;
            std::uint8_t  _pad = 0;
            std::uint32_t operand_offset;   // Index into the operand pool.
            std::uint32_t byte_offset;      // Original byte offset in code section.
            std::uint32_t byte_size;        // Encoded size in bytes.
        };

        // Intermediate result from a single decode step.
        // Returned by the user's Decoder implementation.
        export struct Decoded
        {
            std::uint16_t opcode;
            std::uint8_t  operand_count;
            std::uint32_t byte_size;
            std::array<Operand, MaxOperands> operands{};
        };

        // Lifted program: flat contiguous arrays, no per-instruction allocations.
        export struct Program
        {
            std::vector<Insn>    code;       // Decoded instruction stream.
            std::vector<Operand> pool;       // Shared operand pool.
            std::vector<std::byte> data;     // Copy of the data section bytes.

            std::uint64_t entry_ip    = 0;   // Instruction index of entry point.
            std::uint64_t code_vaddr  = 0;   // Virtual address base of code section.
            std::uint64_t data_vaddr  = 0;   // Virtual address base of data section.

            // Resolve a code virtual address to an instruction index.
            constexpr auto resolve_vaddr(std::uint64_t vaddr) const
                -> std::optional<std::size_t>
            {
                const auto offset = vaddr - this->code_vaddr;

                // Binary search over sorted byte_offset fields.
                auto it = std::ranges::lower_bound(this->code, static_cast<std::uint32_t>(offset), {}, &Insn::byte_offset);

                if(it != this->code.end() && it->byte_offset == offset)
                {
                    return static_cast<std::size_t>(std::distance(this->code.begin(), it));
                };

                return std::nullopt;
            };

            // Read a data value at the given virtual address.
            template<class T> constexpr auto read_data(std::uint64_t vaddr) const
                -> std::optional<T>
              requires std::is_trivially_copyable_v<T>
            {
                const auto offset = vaddr - this->data_vaddr;

                if(offset + sizeof(T) > this->data.size())
                {
                    return std::nullopt;
                };

                return xxas::decode<T>(std::span{ this->data }.subspan(offset, sizeof(T)));
            };

            // Get the operand span for an instruction.
            constexpr auto operands_of(const Insn& insn) const
                -> std::span<const Operand>
            {
                return std::span{ this->pool }.subspan(insn.operand_offset, insn.operand_count);
            };
        };

        // Decoder concept: user provides the inverse of their Encoder.
        //   decode(bytes)  → decoded instruction, or nullopt on failure.
        export template<class D>
        concept Decoder = requires(const D decoder, std::span<const std::byte> bytes)
        {
            { decoder.decode(bytes) } -> std::same_as<std::optional<Decoded>>;
        };

        // Lifter error classification.
        export enum class Err: std::uint8_t
        {
            NoCodeSection,
            DecodeFailed,
            EntryPoint,
        };

        // Lifter result type.
        export using Result = xxas::Result<Program, Err>;
    };

    /***
     **  Lifter: Binary -> ready to execution program.
     **
     **  Template parameters:
     **    `arch` Architecture descriptor (compile-time constant reference).
     **    `Dec`  Decoder type satisfying lift::Decoder.
     **
     **  Walks code section, calling the decoder for each instruction,
     **  and packs decoded instructions and operands into contiguous arrays.
     **  The resulting lift::Program is ready for direct execution with zero
     **  additional allocations needed in the hot path.
     ***/
    export template<const auto& arch, lift::Decoder Dec>
    struct Lifter
    {
        // Lift a binary into an execution-ready program.
        static constexpr auto from(const Binary& binary, const Dec& decoder)
            -> lift::Result
        {
            // Locate the code section.
            auto code_idx = binary.find_section(Binary::Type::Code);
            if(!code_idx)
            {
                return xxas::error(lift::Err::NoCodeSection, "binary contains no code section");
            };

            auto code_bytes = binary.section_bytes(*code_idx);
            if(!code_bytes)
            {
                return xxas::error(lift::Err::NoCodeSection, "failed to read code section bytes");
            };

            const auto& code_section = binary.sections[*code_idx];

            // Pre-count instructions for reservation.
            // First pass: walk the byte stream to count instructions.
            std::size_t insn_count    = 0;
            std::size_t operand_count = 0;

            {
                std::size_t offset = 0;
                while(offset < code_bytes->size())
                {
                    auto remaining = code_bytes->subspan(offset);
                    auto decoded   = decoder.decode(remaining);

                    if(!decoded)
                    {
                        return xxas::error(lift::Err::DecodeFailed,
                            std::format("decode failed at code offset {}", offset));
                    };

                    insn_count++;
                    operand_count += decoded->operand_count;
                    offset        += decoded->byte_size;
                };
            };

            // Allocate flat arrays in one shot.
            lift::Program program;
            program.code.reserve(insn_count);
            program.pool.reserve(operand_count);
            program.code_vaddr = code_section.vaddr;

            // Second pass: decode and pack.
            std::size_t offset      = 0;
            std::uint32_t pool_head = 0;

            while(offset < code_bytes->size())
            {
                auto remaining = code_bytes->subspan(offset);
                auto decoded   = decoder.decode(remaining);

                // Pack instruction header.
                program.code.push_back(lift::Insn
                {
                    .opcode         = decoded->opcode,
                    .operand_count  = decoded->operand_count,
                    .operand_offset = pool_head,
                    .byte_offset    = static_cast<std::uint32_t>(offset),
                    .byte_size      = decoded->byte_size,
                });

                // Pack operands into shared pool.
                for(std::uint8_t i = 0; i < decoded->operand_count; ++i)
                {
                    program.pool.push_back(decoded->operands[i]);
                };

                pool_head += decoded->operand_count;
                offset    += decoded->byte_size;
            };

            // Copy data section if present.
            if(auto data_idx = binary.find_section(Binary::Type::Data))
            {
                const auto& data_section = binary.sections[*data_idx];
                program.data_vaddr = data_section.vaddr;

                if(auto data_bytes = binary.section_bytes(*data_idx))
                {
                    program.data.assign(data_bytes->begin(), data_bytes->end());
                };
            };

            // Resolve entry point to instruction index.
            auto entry = program.resolve_vaddr(binary.header.entry_point);
            if(!entry)
            {
                return xxas::error(lift::Err::EntryPoint,
                    std::format("entry point 0x{:x} does not map to an instruction", binary.header.entry_point));
            };

            program.entry_ip = *entry;

            return program;
        };
    };
};
