import std;
import xxas;


namespace xxas_tests
{
    constexpr auto find()
    {
        constexpr static xxas::CMap map
        {
            std::pair{"001", 1},
            std::pair{"002", 2},

        };

        auto it_1 = map.find("002");
        xxas::assert_ne(it_1, map.cend());
        xxas::assert_eq(it_1->second, 2);
    };

    constexpr xxas::Tests cmap
    {
        find,
    };
};


int main()
{
    return xxas_tests::cmap();
};
