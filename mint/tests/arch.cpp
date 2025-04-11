import std;
import xxas;
import mint;

namespace mint_tests
{
    using namespace mint;

    constexpr auto kw_regs()
    {
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

        xxas::assert_ne(keywords.find("dword"), keywords.cend());
        xxas::assert_ne(keywords.find("gp0"), keywords.cend());

        // Create a new register file.
        auto registers = keywords.reg_file();

        xxas::assert_ne(registers.find("gp2"), registers.cend());
    };

    constexpr xxas::Tests arch
    {
        kw_regs,
    };
};

int main()
{
    return mint_tests::arch();
};
