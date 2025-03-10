export module xxas: bmap;

import std;
import :meta;
import :fnv1a;

namespace xxas
{
    export template<class K, class T, std::size_t N> struct BMap
    {
        using Hash      = std::size_t;
        using Hasher    = Fnv1a<Hash>;

        using KeyArray  = std::array<Hash, N>;
        using DataArray = std::array<T, N>;

        KeyArray keys;
        DataArray data;

        // Returns an `std::optional` containing a `const T`
        // of a found entry by key `I`.
        constexpr auto find(std::convertible_to<K> auto&& key) const
            -> std::optional<const T>
        {
            auto hash = Hasher::hash<K>(key);
            auto it   = std::lower_bound(keys.cbegin(), keys.cend(), hash,
            [](const Hash& first, const Hash& hash)
            {
                return first < hash;
            });

            if(it != this->keys.cend() && *it == hash)
            {
                return data[std::distance(this->keys.cbegin(), it)];
            };

            return std::nullopt;
        };

        // Constructs a constant evaluation binary search map from `entries`.
        template<std::convertible_to<K>... Ks, std::convertible_to<T>... Ts> consteval BMap(std::pair<Ks, Ts>&&... entries)
        {
            std::invoke([&]<auto... In>(std::index_sequence<In...>)
            {
                this->keys = KeyArray
                {   // Hash of each key.
                    Hasher::hash<K>(entries.first)...
                };

                // Sort the keys by size for optimized binary searching.
                std::ranges::sort(this->keys, [](const Hash& first, const Hash& second)
                {
                    return first < second;
                });

                // Move each of the data entries.
                this->data =
                {
                    std::move(entries.second)...
                };
            }, std::make_index_sequence<N>{});
        };
    };

    export template<class... K, class... T> BMap(std::pair<K, T>&&...)
        -> BMap<K...[0], T...[0], sizeof...(K)>;
};
