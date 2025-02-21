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
 **  module:   xxas: operand
 **  purpose:  High-level expression-based operand with meta data information.
 **
 *** **/

namespace mint
{
    export struct Operand
    {
        Expression expression;
        Traits     traits;

        enum class MapErrs: std::uint8_t
        {
            Uninitialized,
            Casting,
        };

        using MapErr      = xxas::Error<MapErrs, Memory::MemErr>;
        using MapResult   = std::expected<Scalar, MapErr>;

        template<class T> constexpr static inline std::array map
        {
            +[](const T& scalar, Teb& teb)
                -> MapResult
            {
                auto thread_file = teb.thread_file();

                // Get the register id from the scalar.
                auto regid = scalar.template as<std::size_t>();

                // Extract the underlying bytes of the register from regid.
                return Scalar
                {
                    thread_file.get().reg_file[regid]
                };
            },
            // Immediate value.
            +[](const T& scalar, Teb& teb)
                -> MapResult
            {
                return scalar;
            },
            // Memory address.
            +[](const T& scalar, Teb& teb)
                -> MapResult
            {   // Get the virtual address from the scalar.
                auto vaddr = scalar.template as<std::uintptr_t>();

                // Get the physical address from the virtual address.
                auto paddr_result = teb.peb().mem().translate(vaddr);

                if(!paddr_result)
                {
                    return MapErr::from(paddr_result);
                };

                

                return Scalar
                {
                    
                };
            },
        };
    };
};
