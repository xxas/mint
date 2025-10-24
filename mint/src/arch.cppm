export module mint: arch;

import std;
import xxas;
import :scalar;
import :traits;

/*** **
 **
 **  module:   mint: arch
 **  purpose:  High-level instruction set architecture definitions.
 **
 *** **/

namespace mint
{
    export using Registers = std::unordered_map<std::string_view, std::vector<std::byte>>;

    namespace arch
    {   // User-defined keywords.
        export template<std::size_t N> struct Keywords
            : xxas::CMap<std::string_view, Traits, N>
        {
            // User-defined keywords are stored in a compile-time O(1) lookup map using minimal perfect hashing.
            // During runtime we lookup registers and map them to their correct bitness using CMap::find.
            using Base  = xxas::CMap<std::string_view, Traits, N>;
            using Entry = Base::Entry;

            using Base::find;

            template<class... Strs> constexpr Keywords(std::pair<Strs, Traits>... keywords)
                : Base{std::pair{std::string_view(keywords.first), keywords.second}...} {};

            // Filters user-defined keywords that are given the Register trait,
            // zero-initialize each user-defined register to the specified bitness.
            template<class T> auto get(T comp) const
                -> Registers
            {
                auto entries = std::ranges::filter_view(this->entries,
                [comp](const auto& keyword)
                {
                    return keyword.second.template get_as<T>() == comp;
                });

                Registers registers{};

                for(const auto& [string, traits]: entries)
                {   // Insert each register with the proper bitness.
                    registers.insert_or_assign(string, std::vector<std::byte>{ traits.size() });
                };

                return registers;
            };
        };

        template<class... Strs> Keywords(std::pair<Strs, Traits>...)
            -> Keywords<sizeof...(Strs)>;

        // User-defined instruction function alternatives.
        template<class... Ts> using Insn = xxas::meta::DedupExtend_t<std::variant<Ts...[0]>, Ts...>;

        export template<class... Ts> struct Insns
            : xxas::CMap<std::string_view, Insn<Ts...>, sizeof...(Ts)>
        {
            using Insn = Insn<Ts...>;
            using Base = xxas::CMap<std::string_view, Insn, sizeof...(Ts)>;

            using Base::find;

            template<class... Strs> constexpr Insns(std::pair<Strs, Ts>&&... insns)
                : Base{std::pair{std::string_view(std::move(insns.first)), Insn(std::move(insns.second))}...} {};
        };
    };

    export template<class Insns, class Regs> struct Arch;
    template<class... Insns, std::size_t N> struct Arch<arch::Insns<Insns...>, arch::Keywords<N>>
    {   // Instruction Mnemonics -> function variant.
        arch::Insns<Insns...> insns;

        // Register ids -> initialization.
        arch::Keywords<N> keywords;

        constexpr Arch(arch::Insns<Insns...> insns, arch::Keywords<N> keywords)
          : insns(insns), keywords(keywords) {};

        // Initializes a new register file.
        constexpr auto get_registers() const
            -> Registers
        {
            return this->keywords.get(traits::Source::Register);
        };
    };

    template<class... Insns, std::size_t N> Arch(arch::Insns<Insns...>, arch::Keywords<N>)
      -> Arch<arch::Insns<Insns...>, arch::Keywords<N>>;
};
