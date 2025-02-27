export module mint: jit_compiler;

import std;
import xxas;
import :operand;
import :instruction;
import :binding;
import :context;

/*** **
 **
 **  module:   mint: jit_compiler
 **  purpose:  Transforms IR into direct function calls at runtime.
 **
 *** **/

namespace mint
{
    struct JitCompiler
    {
        enum CompilationErrs: std::uint8_t
        {
            Empty, Missing,
        };

        // Jit compiler input instructions.
        using Input = std::vector<Instruction>;

        // Jit compiler output bindings.
        using Output = std::vector<Binding>;

        using CompilationErr    = xxas::Error<CompilationErrs, Operand::EvalErr>;
        using CompilationResult = std::expected<Output, CompilationErr>;

        // Just-in-time compiles loose instruction information into direct function calls for a thread.
        template<xxas::BMap Opcodes, cpu::Initializer Initializer> constexpr static auto from(Input& input, Teb& teb)
            -> CompilationResult
        {
            if(input.empty())
            {
                return CompilationErr::err(CompilationErrs::Empty, "Not enough instructions to preform JIT compilation");
            };

            // JIT output bindings.
            Output output{};
            for(Instruction& insn: input)
            {   // Evaluate each operand.
                auto results = std::ranges::transform(insn.operands, [&](Operand& operand)
                {
                    return operand.evaluate<Initializer>(teb);
                });

                for(auto& result: results)
                {   // Check if any operands failed to evaluate.
                    if(!result)
                    {
                        return CompilationErr::from(result);
                    };
                };

                using EvalResult = Operand::EvalResult;

                // Aggregate successfully evaluated results as scalars.
                auto scalars = std::ranges::transform(results, [&](EvalResult& result)
                {
                    return *result;
                });

                // Get the function for the opcode.
                auto& funct  = Opcodes.find(static_cast<std::size_t>(insn.opcode));

                if(funct == std::nullopt)
                {
                    return CompilationErr::err(CompilationErrs::Missing, std::format("Cannot find a matching function for opcode: {}", insn.opcode));
                };

                // Bind to the function.
                auto binding = funct->visit([&](auto& opcode_funct)
                {
                    return Binding::create(opcode_funct, scalars);
                });
            };

            // Return our built functions.
            return output;
        };
    };
};
