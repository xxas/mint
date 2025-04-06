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

        using KeyPair   = std::pair<Hash, K>;
        using Entry     = std::pair<KeyPair, T>;

        using Array     = std::array<Entry, N>;
        using Iterator  = Array::const_iterator;

        Array entries;

        // Returns an `std::optional` containing a `const T`
        // of a found entry by key `I`.
        template<class In> constexpr auto find(In&& key) const
            -> Iterator
            requires std::convertible_to<In, K>
        {   // Forward the key to the hashing function for computation.
            auto hash = Hasher::hash<K>(std::forward<In>(key));

            // Perform our binary search.
            auto it   = std::lower_bound(this->entries.cbegin(), this->entries.cend(), hash,
            [](const Entry& entry, const Hash& hash)
            {
                return entry.first.first < hash;
            });

            // Check if the key is a match.
            // TODO: Check for collisions via the key pair.
            if(it != this->entries.cend() && it->first.first == hash)
            {
                return it;
            };

            // Collision or hashes aren't a match.
            return this->entries.cend();
        };

        constexpr auto begin() const noexcept
            -> Iterator
        {
            return this->entries.begin();
        };

        constexpr auto end() const noexcept
            -> Iterator
        {
            return this->entries.end();
        };

        constexpr auto cbegin() const noexcept
            -> Iterator
        {
            return this->entries.cbegin();
        };

        constexpr auto cend() const noexcept
            -> Iterator
        {
            return this->entries.cend();
        };

        // Constructs a constant evaluation binary search map from `entries`.
        template<class... Ks, class... Ts> consteval BMap(std::pair<Ks, Ts>&&... entries)
            requires((std::convertible_to<Ks, K> && ...) && (std::convertible_to<Ts, T> && ...))
        {
            this->entries = Array
            {
                [&]
                {   // Compute the hash of the key first.
                    auto hash = Hasher::hash<K>(std::forward<Ks>(entries.first));

                    // Construct the key pair secondly; moving the hash and key into the new struct.
                    auto key_pair = KeyPair(std::move(hash), std::move(entries.first));

                    return Entry(std::move(key_pair), std::move(entries.second));
                }()...
            };

            std::ranges::sort(this->entries, [](const Entry& first, const Entry& second)
            {
                return first.first.first < second.first.first;
            });
        };
    };

    export template<class... K, class... T> BMap(std::pair<K, T>&&...)
        -> BMap<K...[0], T...[0], sizeof...(K)>;
};
