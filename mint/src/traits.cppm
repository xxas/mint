export module mint: traits;

import std;

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
            Src = 0b0, Dest = 0b1
        };

        // Dictates where data can be sourced from.
        export enum class Source: std::uint8_t
        {
            Register = 0b001, Immediate = 0b010,
            Memory   = 0b100, Any       = 0b111,
        };

        export constexpr auto operator|(const Source& first, const Source& second) noexcept
            -> Source
        {
            return static_cast<Source>(std::to_underlying(first) | std::to_underlying(second));
        };

        export constexpr auto operator&(const Source& first, const Source& second) noexcept
            -> std::underlying_type_t<Source>
        {
            return std::to_underlying(first) & std::to_underlying(second);
        };

        // Dictates the maximum supported data bitness.
        export enum class Bitness: std::uint8_t
        {
            b8   = 0b000, b16  = 0b001,
            b32  = 0b010, b64  = 0b011,
            b128 = 0b100, b256 = 0b101,
            b512 = 0b111,
        };

        // Returns the Bitness trait as a integer.
        constexpr auto bitness_for(const Bitness& bitness) noexcept
            -> std::uint16_t
        {
            constexpr static std::array<std::uint16_t, 7> bitness_map
            { 8, 16, 32, 64, 128, 256, 512 };

            return bitness_map[static_cast<std::size_t>(bitness)];
        };

        // Dictates the format of the arithmetic e.g. floating point or integral.
        export enum class Format: std::uint8_t
        {
            Integral = 0b0, Floating = 0b1
        };
    };

    export struct Traits
    {
        traits::Direction   direction: 1;
        traits::Source        sources: 3;
        traits::Bitness       bitness: 3;
        traits::Format         format: 1;

        constexpr auto operator!=(const traits::Source& source) const noexcept
            -> bool
        {
            return (std::to_underlying(this->sources) & std::to_underlying(source)) == 0;
        };

        constexpr auto operator==(const Traits& other) const noexcept
            -> bool
        {
            return this->format == other.format
                && this->bitness >= other.bitness
                && (this->sources & other.sources) != 0;
        };
    };
};

