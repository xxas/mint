export module mint: instance;

import std;
import xxas;

import :ir;
import :object;
import :binary;
import :lowering;
import :lifter;
import :executor;

/*** **
 **
 **  module:   mint: instance
 **  purpose:  High-level pipeline types for building and running programs.
 **            InstanceBuilder collects Objects, merges their IR, lowers
 **            through the encoder, lifts with the decoder, and produces
 **            an Instance ready for inspection and execution.
 **
 **  pipeline: Object::from -> InstanceBuilder::add -> build -> Instance
 **
 *** **/

namespace mint
{
    namespace inst
    {
        export enum class Err: std::uint8_t
        {
            Compilation,
            Linking,
        };

        export using Error = xxas::Error<Err>;
    };

    // Execution-ready program with inspection and execution access.
    export template<const auto& arch, const auto& dispatch> struct Instance
    {
        lift::Program program;

        // Lifted instruction stream.
        constexpr auto code() const
            -> std::span<const lift::Insn>
        {
            return this->program.code;
        };

        // Shared operand pool.
        constexpr auto pool() const
            -> std::span<const lift::Operand>
        {
            return this->program.pool;
        };

        // Operands for a specific instruction.
        constexpr auto operands_of(const lift::Insn& insn) const
            -> std::span<const lift::Operand>
        {
            return this->program.operands_of(insn);
        };

        // Data section bytes.
        constexpr auto data() const
            -> std::span<const std::byte>
        {
            return this->program.data;
        };

        // Execute from the declared entry point.
        auto run()
            -> exec::Result
        {
            return Executor<arch, dispatch>::run(this->program);
        };
    };

    // Builder: collects Objects, merges IR, lowers + lifts into an Instance.
    export template<const auto& arch, const auto& dispatch, lowering::Encoder Enc, lift::Decoder Dec>
    struct InstanceBuilder
    {
        using BuildResult = xxas::Result<Instance<arch, dispatch>, inst::Err>;

        ir::Program combined{};

        // Add a compiled object. Merges its IR nodes into the combined program.
        // If the object declares an entry point, it becomes the program entry.
        template<const auto& rules>
        auto add(Object<arch, rules> obj) -> InstanceBuilder&
        {
            for(auto& node: obj.program.nodes)
            {
                this->combined.nodes.push_back(std::move(node));
            };

            if(!obj.program.entry_point.empty())
            {
                this->combined.entry_point = obj.program.entry_point;
            };

            return *this;
        };

        // Link all objects and build an Instance.
        // Lowers the merged IR through the encoder, then lifts with the decoder.
        auto build() -> BuildResult
        {
            // Lower combined IR into a Binary.
            auto binary = Lowering<arch, Enc>::from(this->combined, Enc{});

            if(!binary)
            {
                return xxas::error(inst::Err::Compilation, std::move(binary.error().message));
            };

            // Lift Binary into an execution-ready Program.
            auto program = Lifter<arch, Dec>::from(*binary, Dec{});

            if(!program)
            {
                return xxas::error(inst::Err::Linking, std::move(program.error().message));
            };

            return Instance<arch, dispatch>{ std::move(*program) };
        };
    };
};
