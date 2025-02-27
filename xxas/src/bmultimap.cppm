export module xxas: bmultimap;

import std;
import :tests;
import :fnv1a;

namespace xxas
{   // Compile-time Binary Search Varying Range Map
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

        DataArray data{};
        KeyArray  keys{};

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
        {
            std::invoke([&]<auto... In>(std::index_sequence<In...>)
            {
                this->keys =
                {   // Hash of each key; and
                    // the original index of each key.
                    std::pair
                    {
                        Hasher::hash(std::get<0>(entries...[In])), In
                    }...
                };

                // Sort the keys by size for optimized binary searching.
                std::ranges::sort(this->keys, [](const KeyPair& first, const KeyPair& second)
                {
                    return first.first < second.first;
                });

                // Move the entry data ranges into a consolidated contigous data structure.
                ((std::ranges::move(std::get<1>(entries...[In]),
                    this->data.begin() + std::accumulate(EntrySizes.begin(), EntrySizes.begin() + In, 0uz)
                )), ...);

            }, std::make_index_sequence<sizeof...(N)>{});
        };
    };

    // Template guide for std::array.
    export template<class K, class T, std::size_t... N> BMultiMap(std::pair<K, std::array<T, N>>&&...)
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

    export template<class K, class T, std::size_t N> constexpr auto entry(K&& key, T(&&data)[N])
    {   // Convert the data to std::array.
        std::array<T, N> array{};
        std::ranges::move(data, array.begin());

        return std::pair
        {
            make_key(std::forward<K>(key)), std::move(array)
        };
    };

    export template<class K, class T, std::size_t N> constexpr auto entry(K&& key, std::array<T, N>&& data)
    {
        return std::pair
        {
            make_key(std::forward<K>(key)), std::move(data)
        };
    };
};
