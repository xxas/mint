export module xxas: multiarray;

import std;

namespace xxas
{   // Contiguous array.
    // data types of `class T`; total size of `std::size_t N`; Array count of `C`; Discriminator data type of `D`.
    export template<class T, std::size_t N, std::size_t C, class D> struct MultiArray
    {
        using Elements       = std::array<T, N>;
        using Discriminator  = D;
        using Discriminators = std::array<Discriminator, C - 1uz>;

        Elements        elements;
        Discriminators  discriminators;

        // Constructs contiguous array of `T` from multiple variable (`S...`) length arrays of `T`
        // uses a discriminator to determine the start and end of each range.
        template<std::size_t... S> constexpr MultiArray(const std::array<T, S>&... arrays) noexcept
            : elements{}, discriminators{}
        {   // Create an array of iterable span; since each array is variable in length. 
            std::array spans
            {
                std::span(arrays.begin(), arrays.size())...
            };

            // Length of initialized elements.
            std::size_t len = 0uz;
            for(const auto& [span, index]: std::views::zip(spans, std::views::iota(0uz)))
            {   // Copy each of the fixed ranges into the contiguous elements.
                std::ranges::copy(span, this->elements.begin() + len);

                // Increment total elements consumed.
                len = len + span.size();

                if(index < (C - 1uz))
                {   // Set the range end discriminator.
                    this->discriminators[index] = len;
                };
            };
        };

        constexpr auto get(const std::size_t& index) const noexcept
            -> std::span<const T>
        {   // Use the previous ranges discriminator to retrieve the current ranges offset into the contiguous elements.
            const std::size_t offset  = index == 0uz ? 0uz : this->discriminators[index - 1uz];

            // Use the offset and discriminator to determine the length of the range.
            const std::size_t len     = index != (C - 1uz) ? this->discriminators[index] - offset: elements.size() - offset;

            // Return a std::span containing the fixed ranges elements.
            return std::span<const T>(this->elements.begin() + offset, len);
        };
    };

    // Template deduction-guide for the MultiArray constructor from a variadic template of std::arrays.
    // This helps deduce the template params `N`, `C` and `D`. 
    template<class T, std::size_t... N> MultiArray(std::array<T, N>...) -> MultiArray<T, (0uz + ... + N), sizeof...(N),
        std::conditional_t<((N > std::numeric_limits<std::uint8_t>::max()) || ...), std::size_t, std::uint8_t>>;
};
