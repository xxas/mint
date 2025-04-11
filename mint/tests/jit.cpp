import std;
import xxas;
import mint;

namespace mint_tests
{
    using namespace mint;

    constexpr auto bindings()
    {
        
    };

    constexpr xxas::Tests jit
    {
        bindings
    };
};

int main()
{
    return mint_tests::jit();
};
