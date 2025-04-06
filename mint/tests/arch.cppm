import std;
import xxas;
import mint;

namespace mint_tests
{
    using namespace mint;

    /*constexpr static arch::Insns insns
    {
        std::pair{"hello_world", []
        {
            std::println("Hello world!");
        }},
        std::pair{"some_int", [](int integer)
        {
            std::println("int is: {}", integer);
        }},
    };

    constexpr auto insn_lookup()
    {   // Should be able to find hello_world.
        xxas::assert_ne(insns.find("hello_world"), insns.cend());
        xxas::assert_eq(insns.find("some_float"), insns.cend());
    };*/

    constexpr auto reg_init()
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

        for(const auto& keyword: keywords)
        {
            std::println("{} {}", keyword.first.second, keyword.second.get<traits::Bitness>());
        };

        // Create a new register file.
        auto registers = keywords.reg_file();

        std::println("{}", registers.size());
    };

    constexpr xxas::Tests arch
    {
        reg_init
    };
};

int main()
{
    return mint_tests::arch();
};
