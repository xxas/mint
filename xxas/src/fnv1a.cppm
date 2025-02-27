export module xxas: fnv1a;

import std;
import :tests;
import :meta;

namespace xxas
{
    struct HashParameters
    {   // Constants for 32-bit and 64-bit FNV hashing.
        using Parameters = std::tuple<std::uint32_t, std::uint64_t>;

        constexpr static inline Parameters Primes  = { 0x01000193, 0x00000100000001b3 };
        constexpr static inline Parameters Offsets = { 0x811c9dc5, 0xcbf29ce484222325 };
    };

    export template<class H> requires(std::same_as<H, std::uint64_t> ||
        std::same_as<H, std::uint32_t>) struct Fnv1a
    {
        using ValueType = H;

        constexpr static inline auto Prime  = std::get<H>(HashParameters::Primes);
        constexpr static inline auto Offset = std::get<H>(HashParameters::Offsets);

        consteval auto prime() const noexcept
            -> const ValueType&
        {
            return Prime;
        };

        consteval auto offset() const noexcept
            -> const ValueType&
        {
            return Offset;
        };

        template <std::ranges::range R> constexpr auto operator()(const R& range) const
          -> const ValueType
        {
            static_assert(std::is_integral_v<std::ranges::range_value_t<R>> ||
                            std::is_same_v<std::ranges::range_value_t<R>, char>,
                            "Hashing is only supported for ranges of integral or character types.");

            ValueType hash = Offset;

            std::ranges::for_each(range, [&](const auto& K)
                { hash = (hash ^ static_cast<H>(K)) * Prime; });

            return hash;
        };

        template<class T> constexpr auto operator()(const T& K) const
          -> const ValueType
        {
            return (Offset ^ static_cast<H>(K)) * Prime;
        };

        template<class T> constexpr static auto hash(const T& K)
            -> const ValueType
        {
            return std::invoke(Fnv1a{}, K);
        };
    };

    export template<class T> constexpr inline Fnv1a<T> fnv1a{};

    export constexpr inline Fnv1a<std::uint32_t> fnv1a_32{};
    export constexpr inline Fnv1a<std::uint64_t> fnv1a_64{};
};
