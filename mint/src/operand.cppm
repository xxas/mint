export module mint: operand;

import std;
import xxas;

import :traits;
import :scalar;
import :context;
import :expression;
import :semantics;

/*** **
 **
 **  module:   mint: operand
 **  purpose:  High-level expression-based operand with meta data information.
 **
 *** **/

namespace mint
{
    export struct Operand
    {
        using Variant = std::variant<Expression, Scalar>;

        Variant    variant;
        Traits     traits;

        enum class EvalErrs: std::uint8_t
        {
            Uninitialized,
            Casting,
        };

        using EvalErr      = xxas::Error<EvalErrs, Memory::MemErr>;
        using EvalResult   = std::expected<Scalar, EvalErr>;

        constexpr static inline std::array source_map
        {   // Register from scalar.
            +[](Traits& _, const Expression& expression, Teb& teb)
                -> EvalResult
            {   // Get the register id from the scalar.
                auto regid = expression.template evaluate<std::size_t>();

                // Get the cpu thread file through TEB.
                auto thread_file = teb.thread_file();

                auto& reg = thread_file.get().reg_file[regid];

                // Extract the underlying bytes of the register from regid.
                return Scalar
                {{
                    reg.data(), reg.size()
                }};
            },
            // Immediate value from scalar.
            +[](Traits& traits, const Expression& expression, Teb& teb)
                -> EvalResult
            {
                return EvalErr::err(EvalErrs::Casting, "Unable to cast immediate value to evaluatable result");
            },
            // Memory address from scalar.
            +[](Traits& traits, const Expression& expression, Teb& teb)
                -> EvalResult
            {   // Get the virtual address from the scalar.
                auto vaddr = expression.template evaluate<std::uintptr_t>();

                // Get the physical address from the virtual address.
                auto paddr_result = teb.peb().mem().translate(vaddr);

                if(!paddr_result)
                {
                    return EvalErr::from(paddr_result);
                };

                // Get the data from the physical address.
                std::byte* data_ptr = reinterpret_cast<std::byte*>(*paddr_result);

                return Scalar
                {{  // Use the bitness provided by traits to get the correct size.
                    data_ptr, traits::bitness_for(traits.bitness)
                }};
            },
        };

        auto evaluate(Teb& teb)
            -> EvalResult
        {
            auto expression = [&](Expression& expr)
                -> EvalResult
            {   // Retrieve the source extraction function for the type of source.
                auto source_funct = source_map[static_cast<std::size_t>(this->traits.sources)];

                return std::invoke(source_funct, this->traits, expr, teb);
            };

            auto constant   = [](Scalar& scalar)
                -> EvalResult
            {
                return std::forward<Scalar>(scalar);
            };

            // Evaluate the expression and retrieve the scalar based on its provided traits.
            return this->variant.visit(xxas::meta::Overloads
            {
                expression, constant
            });
        };
    };
};
