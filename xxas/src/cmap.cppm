

export module xxas: cmap;

import std;
import :meta;
import :fnv1a;

namespace xxas
{
    export template<class K, class T, std::size_t N> struct CMap
    {
        using Hash      = std::uint64_t;
        using Hasher    = Fnv1a<Hash>;
        using Entry     = std::pair<K, T>;
        using Entries   = std::array<Entry, N>;
        using Indicing  = std::array<std::size_t, N>;

        constexpr static std::size_t NPos = std::numeric_limits<std::size_t>::max();

        Entries  entries{};
        Indicing index{};
        Hash     mask{};

        template<class... Ks, class... Ts> consteval CMap(std::pair<Ks, Ts>&&... in)
            requires(std::convertible_to<Ks, K> && ...) && (std::convertible_to<Ts, T> && ...)
        {
            // Precompute the hashes of each entry.
            std::array<Hash, N> hashes =
            {
                Hasher::hash<K>(std::forward<Ks>(in.first))...
            };

            // Move ownership of entry pairs into a std::array for range-based algorithms.
            this->entries = Entries
            {
                std::make_pair(std::move(in.first), std::move(in.second))...
            };

            // Derive a perfect mask from hashes.
            this->mask = derive_mask(hashes);

            // Fill and map indices deterministically.
            std::ranges::fill(this->index, NPos);
            for(std::size_t i = 0; i < N; ++i)
            {
                const auto slot = hashes[i] & this->mask;
                this->index[slot % N] = i;
            };
        };

        static consteval Hash derive_mask(std::span<const Hash, N> hashes)
        {
            Hash mask = ~Hash{0};
            for(int bit = 63; bit >= 0; --bit)
            {
                const Hash test = mask & ~(Hash{1} << bit);
                std::array<Hash, N> proj{};
                bool unique = true;

                for(std::size_t i = 0; i < N; ++i)
                {
                    proj[i] = hashes[i] & test;
                };

                // Check for uniqueness.
                for(std::size_t i = 0; i < N && unique; ++i)
                {
                    for (std::size_t j = i + 1; j < N; ++j)
                    {
                        if (proj[i] == proj[j])
                        {
                            unique = false;
                        };
                    };
                };

                if(unique)
                {
                    mask = test;
                };
            };
            return mask;
        }

        // 
        template<class In> constexpr auto find(In&& key) const
            requires std::convertible_to<In, K>
        {
            const auto hash = Hasher::hash<K>(std::forward<In>(key));
            const auto slot = hash & this->mask;
            const auto pos  = slot % N;

            const auto idx = this->index[pos];
            if(idx == NPos || this->entries[idx].first != key)
            {
                return this->end();
            };

            return this->begin() + idx;
        }

        constexpr auto begin() const noexcept
        {
            return entries.begin();
        };

        constexpr auto end() const noexcept
        {
            return entries.end();
        };

        constexpr auto cbegin() const noexcept
        {
            return entries.cbegin();
        };

        constexpr auto cend() const noexcept
        {
            return entries.cend();
        };
    };

    export template<class... Ks, class... Ts> CMap(std::pair<Ks, Ts>&&...)
        -> CMap<Ks...[0], Ts...[0], sizeof...(Ks)>;

    export template<class... Ks, class... Ts> requires meta::same_as<std::remove_cvref_t<Ks...[0]>, const char*> CMap(std::pair<Ks, Ts>&&...)
        -> CMap<std::string_view, Ts...[0], sizeof...(Ks)>;
}

