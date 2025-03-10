import std;
import xxas;
import mint;

namespace mint_tests
{
    using namespace mint;

    constexpr static arch::Insns insns
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
        xxas::assert_ne(insns.find("hello_world"), std::nullopt);
        xxas::assert_eq(insns.find("some_float"), std::nullopt);
    };

    constexpr auto reg_init()
    {

    };

    constexpr xxas::Tests arch
    {
        insn_lookup,
    };
};

int main()
{
    return mint_tests::arch();
};
