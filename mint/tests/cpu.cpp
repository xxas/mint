import std;
import xxas;
import mint;


namespace mint_tests
{
    using namespace mint;

    constexpr auto reg_init()
    {

    };

    constexpr xxas::Tests cpu
    {
        reg_init
    };
};

int main()
{
    return mint_tests::cpu();
};
