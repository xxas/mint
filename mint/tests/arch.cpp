import std;
import xxas;
import mint;

namespace mint_tests
{
    using namespace mint;

    constexpr static auto keywords = arch::Keywords
    {   // Registers.
        std::pair{"gp0", Traits{traits::Bitness::b64, traits::Source::Register}},
        std::pair{"gp1", Traits{traits::Bitness::b64, traits::Source::Register}},
        std::pair{"gp2", Traits{traits::Bitness::b64, traits::Source::Register}},

        // Misc keywords.
        std::pair{"dword", Traits{traits::Bitness::b64}},
        std::pair{"word",  Traits{traits::Bitness::b32}},
        std::pair{"ptr",   Traits{traits::Source::Memory}},
    };

    constexpr static auto insns = arch::Insns
    {
        std::pair{"println",         [](const auto& src) -> void { std::println("src: type: {}, value: {}", typeid(decltype(src)).name(), src); }},
        std::pair{"mov", [](auto& dest, const auto& src) -> void { dest = src; }}
    };

    constexpr static auto arch = Arch
    {
        insns, keywords
    };

    constexpr void kw_regs()
    {
        xxas::assert_ne(keywords.find("dword"), keywords.cend());
        xxas::assert_ne(keywords.find("gp0"), keywords.cend());

        // Allocate zerod bytes for each register.
        auto registers    = arch.get_registers();

        // Assert that "gp2" exists within the map of registers.
        auto gp2_register = registers.find("gp2");
        xxas::assert_ne(registers.find("gp2"), registers.cend());

        // Assert the bitness of the register.
        auto gp2_bitness = gp2_register->second.size();
        xxas::assert_eq(gp2_bitness, 8u);
    };

    constexpr void find_insns()
    {
        auto println_insn = insns.find("println");
        xxas::assert_ne(println_insn, insns.cend());
    };

    void alloc_regs()
    {
        auto registers = arch.get_registers();

        // Assert that "gp2" exists within the runtime map.
        auto gp2_register = registers.find("gp2");
        xxas::assert_ne(gp2_register, registers.end());

        // Assert the total bytes occupied by the "gp2" register.
        auto gp2_byte_count = gp2_register->second.size();
        xxas::assert_eq(gp2_byte_count, 8u);
    };

    constexpr xxas::Tests architecture
    {
        kw_regs, find_insns, alloc_regs,
    };
};

int main()
{
    return mint_tests::architecture();
};
