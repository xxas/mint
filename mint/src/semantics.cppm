export module mint: semantics;

import std;
import xxas;
import :traits;

/*** **
 **
 **  module:   mint: semantics
 **  purpose:  Building high-level semantical guide systems for user-defined
 **            instruction set architectures.
 *** **/

namespace mint
{
    namespace semantics
    {   // Traits that a set of data should follow.
        export struct Guide
        {
            enum class Format: std::uint8_t
            {
                Fixed = 0b0, Range = 0b1
            };

            struct Fixed
            {
                std::uint16_t size: 15;
                Format      format: 1;

                constexpr Fixed(std::uint16_t size)
                  : size{ size }, format{ Format::Fixed } {};
            };

            struct Range
            {
                std::uint8_t min: 8;
                std::uint8_t max: 7;
                Format    format: 1;

                constexpr Range(std::uint8_t min, std::uint8_t max)
                  : min{ min }, max{ max }, format{ Format::Range } {};
            };

            // The data set cardinal; this defines the size or range of the data set.
            union Cardinal
            {
                Fixed fixed;
                Range range;

                constexpr Cardinal() : fixed{0} {};
                constexpr Cardinal(Fixed&& f) : fixed{ std::move(f) } {};
                constexpr Cardinal(Range&& r) : range{ std::move(r) } {};

                constexpr Cardinal(const std::uint16_t& len) : fixed{ Fixed(len) } {};
                constexpr Cardinal(const std::uint8_t& min, const std::uint8_t& max) : range{ Range(min, max) } {};
            } cardinal{};

            // Traits for the guide to follow.
            Traits traits{};

            // Returns the format for the guide.
            constexpr auto format() const noexcept
                -> const Format
            {
                return this->cardinal.fixed.format;
            };

            // Validates the range of operands--with an offset of N into the range--against the traits of the guide.
            constexpr auto valid(const std::ranges::range auto& range, const std::size_t& n) const noexcept
                -> const std::size_t
            {   // Get the format bit of the guide.
                const auto format = this->format();

                // Validate the bounds for a set of operands.
                auto bounds = [&](const std::size_t& lower, const std::size_t& upper)
                    -> std::size_t
                {   // Size does not surpass lower bound.
                    if(range.size() < (n + lower))
                    {
                        return 0uz;
                    };

                    // Total elements that are meet the bounds.
                    const auto min   = std::min(upper, range.size() - n);
                    const auto begin = range.begin() + n;
                    const auto end   = begin + min;

                    // Get the subrange of the lower and upper bound.
                    const auto subrange = std::ranges::subrange(begin, end);

                    // Validate that each operand follows the specified traits.
                    for(const auto& operand: subrange)
                    {
                        if(this->traits != operand.traits)
                        {
                            return 0uz;
                        };
                    };

                    return min;
                };

                switch(format)
                {
                    case Format::Fixed:
                    {   // lower and upper bound are concisely fixed.
                        return bounds(this->cardinal.fixed.size, this->cardinal.fixed.size);
                    };
                    case Format::Range:
                    {   // lower and upper loosely define a range of possible counts of arguments.
                        return bounds(this->cardinal.range.min, this->cardinal.range.max);
                    };
                };
            };
        };

        // Validate an std::span of guides against a std::ranges::range of operands.
        export template<std::size_t N> constexpr auto valid(const std::span<const Guide, N>& span, const std::ranges::range auto& range) noexcept
            -> bool
        {   // Total count of validated objects.
            std::size_t count = 0;

            // Validate each data set against a guide.
            for(const Guide& guide: span)
            {
                if(std::size_t result = guide.valid(range, count); result != 0uz)
                {   // Increment the validated count by the result.
                    count = count + result;
                }
                else
                {   // Failed to follow the guide.
                    return false;
                };
            };

            // Return if the entirety of the range was valid.
            return count == std::ranges::size(range);
        };
    };
};
