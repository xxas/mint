export module mint: traits;

import std;
import xxas;

/*** **
 **
 **  module:   mint: traits
 **  purpose:  Defines high-level operand traits, describing the explicit
 *             constraints and implicit characteristics of assembly operands.
 **
 *** **/

namespace mint
{
    namespace traits
    {
        export enum class Direction: std::uint8_t
        {
            Src   = 0b00000000, Dest = 0b10000000,
            Mask  = 0b10000000,
        };

        // Dictates where data can be sourced from.
        export enum class Source: std::uint8_t
        {
            Register = 0b00010000, Immediate = 0b00100000,
            Memory   = 0b01000000, Any       = 0b01110000,
            None     = 0b00000000, Mask      = 0b01110000
        };

        // Dictates the maximum supported data bitness.
        export enum Bitness: std::uint8_t
        {
            b8   = 0b00000010, b16  = 0b00000100,
            b32  = 0b00000110, b64  = 0b00001000,
            b128 = 0b00001010, b256 = 0b00001100,
            b512 = 0b00001110, Mask = 0b00001110,
        };

        // Dictates 
        export enum class Format: std::uint8_t
        {
            Integral = 0b00000000, Floating = 0b00000001,
            Mask     = 0b00000001,
        };

        template<class T, class... Ts> constexpr auto popcount()
            -> bool
        {   // Count the amount of bits flipped to '1'.
            constexpr static auto mask_count      = std::popcount(std::to_underlying(T::Mask));

            // Count the appearances of a type.
            constexpr static auto provided_count  = (0 + ... + std::is_same_v<T, Ts>);

            // Compare the mask popcount against the count of arguments
            // for a bitfield provided.
            return mask_count >= provided_count;
        };

        // Returns that all arguments provided appear less times than the argument type popcount.
        // e.g. popcount(type) >= appearance(type)
        template<class... Ts> constexpr auto provided_count()
            -> bool
        {
            return (... && popcount<Ts, Ts...>());
        };
    };

    export struct Traits
    {   // Underlying bits for each trait.
        std::uint8_t bits;

        template<xxas::meta::enumerable... Ts> consteval Traits(const Ts... enums)
          : bits{static_cast<std::uint8_t>((std::uint8_t{0} | ... | static_cast<std::uint8_t>(std::to_underlying(enums))))}
        {   // Propagate a static assertion if any provided argument appearance exceeds its types popcount.
            static_assert(traits::provided_count<Ts...>(), "Enumerable type appearance exceeds the popcount for its type.");
        };

        // Returns the template argument as its underlying bits.
        template<xxas::meta::enumerable T>constexpr auto get() const noexcept
        {   // Convert the mask to its underlying value on compile time.
            constexpr static auto mask = std::to_underlying(T::Mask);

            // Return the masked bits.
            return this->bits & mask;
        };

        // Returns the template argument as its enum type.
        template<xxas::meta::enumerable T> constexpr auto get_as() const noexcept
        {   // Return the masked bits casted to the enum type.
            return static_cast<T>(this->get<T>());
        };

        // Returns the scalars size in bytes.
        constexpr auto size() const noexcept
            -> std::size_t
        {   // Get the bitness index.
            std::uint8_t index = (this->get<traits::Bitness>()) >> 1;

            // Raise to a power of two.
            return (1 << (index - 1));
        };

        // Ensures compatibility with the provided `other` traits.
        constexpr auto compat(const Traits& other) const noexcept
        {   // Ensure format is equal, bitness isn't exceeded, and sources intersect.
            bool eq_format  = this->get<traits::Format>() == other.get<traits::Format>();
            bool eq_bitness = this->get<traits::Bitness>() >= other.get<traits::Bitness>();
            bool eq_source  = (this->get<traits::Source>() & other.get<traits::Source>()) != 0;

            return eq_source && eq_bitness && eq_format;
        };
    };
};

