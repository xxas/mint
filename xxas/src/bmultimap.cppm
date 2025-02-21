export module xxas: bmultimap;

import std;
import :tests;
import :fnv1a;

namespace xxas
{   // Compile-time Binary Search Multi Map:
    //  * A contiguous array of data allowing for arbitrarily-sized arrays for entries.
    //  * Presorts elements on construction for binary search, O(log n) find complexity.
    export template<class K, class T, std::size_t... N> struct BMultiMap
    {
        using SizesType = std::array<std::size_t, sizeof...(N)>;
        using Hash      = std::size_t;
        using Hasher    = Fnv1a<Hash>;

        constexpr static inline std::size_t TotalElements   = (0 + ... + N);
        constexpr static inline SizesType   EntrySizes      = { N... };

        using KeyPair   = std::pair<Hash, std::size_t>;
        using DataArray = std::array<T, TotalElements>;
        using KeyArray  = std::array<KeyPair, sizeof...(N)>;

        using ConstDataIterator = typename DataArray::const_iterator;

        DataArray data;
        KeyArray  keys;

        // Total amount of elements store continously.
        constexpr auto total() const noexcept
            -> const std::size_t&
        {
            return TotalElements;
        };

        // Returns the size of an entry at the index `in`.
        constexpr auto size(const std::size_t& in) const noexcept
            -> const std::size_t&
        {
            return EntrySizes.at(in);
        };

        // Returns an `std::span<const T>` containing the data for the entry at index `I`.
        constexpr auto at(const std::size_t& in) const
            -> std::span<const T>
        {
            std::size_t start = std::accumulate(EntrySizes.begin(), EntrySizes.begin() + in, 0uz);
            return { &this->data[start], this->size(in) };
        };

        // Returns an `std::optional` containing the `std::span<const T>`
        // of a found entry by key `I`.
        constexpr auto find(const K& key) const
            -> std::optional<std::span<const T>>
        {
            auto hash = Hasher::hash(key);
            auto it   = std::lower_bound(keys.cbegin(), keys.cend(), hash,
            [](const KeyPair& pair, const Hash& hash)
            {
                return pair.first < hash;
            });

            if(it != this->keys.cend() && it->first == hash)
            {
                return this->at(it->second);
            };

            return std::nullopt;
        };

        // Constructs a constant evaluation multi map from `entries`.
        template<class... Entries> consteval BMultiMap(Entries&&... entries)
          requires (sizeof...(Entries) == sizeof...(N))
            : keys{ [&]<auto... In>(std::index_sequence<In...>)
                    {   // Track each key and the index it was found at.
                        KeyArray keys
                        {
                            std::pair
                            {
                                Hasher::hash(std::get<0>(entries...[In])), In
                            }...
                        };

                        // Sort the key pairs for binary search.
                        std::ranges::sort(keys, [](const KeyPair& A, const KeyPair& B)
                        {
                            return A.first < B.first;
                        });
 
                        return keys;
                    }(std::make_index_sequence<sizeof...(N)>{}),
                },
              data{ [&]()
                    {
                        auto copy = [&]<std::size_t... In>(std::index_sequence<In...>, std::array<T, TotalElements> result = {})
                        {
                            ((std::ranges::copy(std::get<1>(entries...[In]),
                                result.begin() + std::accumulate(EntrySizes.begin(), EntrySizes.begin() + In, 0uz)
                            )), ...);

                            return result;
                        };

                        // Copy the entries into a contiguous array of data.
                        return std::invoke(copy, std::make_index_sequence<sizeof...(N)>{});
                    }()
                } {};
    };

    // Template guide for std::array.
    export template<class K, class T, std::size_t... N> BMultiMap(std::pair<K, std::array<T, N>>...)
        -> BMultiMap<K, T, N...>;

    //  Generate a hash key for the input data.
    export template<class K> constexpr auto make_key(const K& key)
    {   // Convert ranges of chars to std::string.
        constexpr bool is_chars = std::is_array_v<K> && std::same_as<std::remove_extent_t<K>, char>;

        return std::conditional_t<is_chars, std::string, K>
        {
            key
        };
    };

    export template<class K, class T, std::size_t N> constexpr auto entry(const K& key, const T (&data)[N])
    {   // Convert the data to std::array.
        std::array<T, N> array{};
        std::ranges::copy(data, array.begin());

        return std::pair{ make_key(key), array };
    };

    export template<class K, class T, std::size_t N> constexpr auto entry(const K& key, const std::array<T, N>& data)
    {
        return std::pair{ make_key(key), data };
    };
};
