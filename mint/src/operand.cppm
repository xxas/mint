export module mint: operand;

import std;
import xxas;

import :traits;
import :scalar;
import :context;
import :expression;
import :semantics;
import :arch;

/*** **
 **
 **  module:   mint: operand
 **  purpose:  High-level expression-based operand with metadata.
 **
 *** **/

namespace mint
{
    export struct Operand
    {
        Expression expression;
        Traits     traits;

        enum class EvalErr: std::uint8_t
        {
            Uninitialized,
            Casting,
            Nonconstant,
        };

        using EvalResult   = xxas::Result<Scalar, EvalErr, Memory::MemErr>;

        template<Arch A> constexpr static inline std::array source_map
        {   // Register from scalar.
            +[](Traits& _, const Expression& expression, Teb& teb)
                -> EvalResult
            {   // Get the register id from the scalar.
                auto regid = expression.evaluate<std::size_t>();

                // Get the cpu thread file through TEB.
                auto thread_file = teb.thread_file<A>();

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
                return expression.constant().transform([](Scalar scalar)
                    -> EvalResult
                {
                    return scalar;
                })
                .value_or(xxas::error(EvalErr::Nonconstant, "Immediate value expects a constant expression"));
            },
            // Memory address from scalar.
            +[](Traits& traits, const Expression& expression, Teb& teb)
                -> EvalResult
            {   // Get the virtual address from the scalar.
                auto vaddr = expression.evaluate<std::uintptr_t>();

                // Get the physical address from the virtual address.
                auto paddr_result = teb.peb().mem().translate(vaddr);

                if(!paddr_result)
                {
                    return paddr_result.error();
                };

                // Get the data from the physical address.
                auto data_ptr = reinterpret_cast<std::byte*>(*paddr_result);

                return Scalar
                {{  // Use the bitness provided by traits to get the correct size.
                    data_ptr, traits.size()
                }};
            },
        };

        template<Arch A> auto evaluate(Teb& teb)
            -> EvalResult
        {   // Get the source function for the traits of the operand.
            auto& source_funct = source_map<A>[static_cast<std::size_t>(this->traits.get<traits::Source>())];

            // Return the evaluated result from the source function.
            return std::invoke(source_funct, this->traits, this->expression, teb);
        };
    };
};
