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
    namespace arch
    {
        export using Register  = std::vector<std::byte>;
        export using RegFile   = std::unordered_map<std::string, Register>;

        export struct ThreadFile
        {   // Current instruction being executed.
            std::size_t ip{};

            // Raw underlying bytes for each register file.
            RegFile reg_file;
        };

        // User-defined keywords.
        export template<std::size_t N> struct Keywords
            : xxas::BMap<std::string, Traits, N>
        {
            using Base  = xxas::BMap<std::string, Traits, N>;
            using Entry = Base::Entry;

            using Base::find;

            template<class... Strs> constexpr Keywords(std::pair<Strs, Traits>&&... keywords)
                : Base{std::pair{std::string(std::move(keywords.first)), std::move(keywords.second)}...} {};

            // Filters user-defined keywords that are given the Register trait,
            // zero-initialize each user-defined register to the specified bitness.
            auto reg_file() const
                -> RegFile
            {
                auto registers = std::ranges::filter_view(this->entries,
                [](const auto& keyword)
                {
                    return keyword.second.template get_as<traits::Source>() == traits::Source::Register;
                });

                RegFile reg_file{};

                for(auto& [key_pair, traits]: registers)
                {   // Insert each register with the proper bitness.
                    reg_file.insert_or_assign(key_pair.second, Register{ traits.size() });
                };

                return reg_file;
            };
        };

        template<class... Strs> Keywords(std::pair<Strs, Traits>&&...)
            -> Keywords<sizeof...(Strs)>;

        // User-defined instruction function alternatives.
        template<class... Ts> using Insn = xxas::meta::DedupExtend_t<std::variant<>, Ts...>;

        export template<class... Ts> struct Insns
            : xxas::BMap<std::string, Insn<Ts...>, sizeof...(Ts)>
        {
            using Insn = Insn<Ts...>;
            using Base = xxas::BMap<std::string, Insn, sizeof...(Ts)>;

            using Base::find;

            template<class... Strs> constexpr Insns(std::pair<Strs, Ts>&&... insns)
                : Base{std::pair{std::string(std::move(insns.first)), Insn(std::move(insns.second))}...} {};
        };
    };

    export template<class Insns, class Regs> struct Arch;
    template<class... Insns, class... Regs> struct Arch<arch::Insns<Insns...>, std::tuple<Regs...>>
    {   // Instruction Mnemonics -> function variant.
        arch::Insns<Insns...> insns;

        // Register ids -> initialization.
        arch::Keywords<sizeof...(Regs)> keywords;

        // Initializes a new register file.
        constexpr auto reg_file() const
            -> arch::RegFile
        {
            return this->keywords.reg_file();
        };
    };
};
