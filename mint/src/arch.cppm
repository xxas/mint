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
    {   // User-defined instruction function alternatives.
        template<class... Ts> using Insn = xxas::meta::DedupExtend_t<std::variant<>, xxas::meta::Template<Ts...>>;

        export using Register  = std::vector<std::byte>;
        export using RegFile   = std::unordered_map<std::string, Register>;

        export struct ThreadFile
        {   // Current instruction being executed.
            std::size_t ip{};

            // Raw underlying bytes for each register file.
            RegFile reg_file;
        };

        // User-defined register-file initialization information.
        export template<std::size_t N> struct RegInit
        {
            using RegBitness = std::array<std::pair<std::string, std::uint16_t>, N>;
            const RegBitness registers;

            // Initialize a new register file based on the bitness fields.
            auto operator()() const noexcept
                -> RegFile
            {
                RegFile reg_file{};

                for(std::size_t i = 0; i < this->registers.size(); i++)
                {
                    auto& [name, bitness_index] = this->registers[i];

                    // Get the bitness for the register.
                    auto bitness = traits::bitness_for(bitness_index);

                    Register reg{};
                    reg.resize(bitness);

                    // Initialize the register to the user-specified bitness.
                    reg_file.insert_or_assign(name, std::move(reg));
                };

                return reg_file;
            };
        };

        export template<class... Ts> struct Insns
            : xxas::BMap<std::string, Insn<Ts...>, sizeof...(Ts)>
        {
            using Insn = Insn<Ts...>;
            using Base = xxas::BMap<std::string, Insn, sizeof...(Ts)>;

            using Base::Base;

            // Construct an array of pointers to each function.
            template<class... Strs> constexpr Insns(std::pair<Strs, Ts>&&... insns)
                requires(std::convertible_to<Strs, std::string> || ...)
                : Base{std::pair{std::string(std::move(insns.first)), Insn(std::move(insns.second))}... } {};
        };
    };

    export template<class Insns, class Regs> struct Arch;
    template<class... Insns, class... Regs> struct Arch<arch::Insns<Insns...>, std::tuple<Regs...>>
    {   // Instruction Mnemonics -> function variant.
        arch::Insns<Insns...> insns;

        // Register ids -> initialization.
        arch::RegInit<sizeof...(Regs)> reg_init;
    };
};
