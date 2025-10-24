export module mint: jit_compiler;

import std;
import xxas;
import :operand;
import :instruction;
import :binding;
import :context;
import :arch;

/*** **
 **
 **  module:   mint: jit_compiler
 **  purpose:  Transforms high-level instruction IR into
 **            directly executable functional bindings.
 **
 *** **/

namespace mint
{
    export struct JitCompiler
    {
        enum Err: std::uint8_t
        {
            Missing,
        };

        // Jit compiler input instructions.
        using Input = std::vector<Instruction>;

        // Jit compiler output bindings.
        using Output = std::vector<Binding>;
        using Result = xxas::Result<Output, Err>;

        // Just-in-time compiles loose instruction information into direct function calls for a thread.
        template<const auto& arch> constexpr static auto from(const Input& input, const ThreadContext<arch>& ctx)
            -> Result
        {   // JIT output bindings.
            Output output{};
            for(const auto& insn: input)
            {   // Evaluate each operand.
                auto results = std::ranges::transform(insn.operands, [&](Operand& operand)
                {
                    return operand.evaluate(ctx);
                });

                // Return the first error found in the evaluated operands.
                if(auto it = std::ranges::find_if(results, std::not_fn(&Operand::Result::has_value)); it != results.end())
                {
                    return it->error();
                };

                // Aggregate successfully evaluated results as scalars.
                auto scalars = std::ranges::transform(results, [&](auto& result)
                {
                    return *result;
                });

                // Find the iterator for the instruction.
                auto funct_it  = arch.insns.find(insn.opcode);

                if(funct_it == arch.insns.cend())
                {
                    return xxas::error(Err::Missing, std::format("Cannot find a matching function for opcode: {}", insn.opcode));
                };

                // Create a new binding for the function.
                auto binding = funct_it->visit([&scalars](auto& funct)
                {
                    return Binding::create(funct, scalars);
                });

                output.push_back(binding);
            };
            // Return our built functions.
            return output;
        };
    };
};
