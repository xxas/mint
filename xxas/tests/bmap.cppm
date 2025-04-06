import std;
import xxas;

namespace xxas_tests
{
    constexpr auto find_basic()
    {
        constexpr static xxas::BMap<std::string, int, 2> map
        {
            std::pair{ "123", 123 },
            std::pair{ "321",  321 },
        };

        constexpr auto it_1 = map.find("123");
        auto it_2           = map.find("321");
        auto it_3           = map.find("132");

        xxas::assert_ne(it_1, map.cend());
        xxas::assert_ne(it_2, map.cend());
        xxas::assert_eq(it_3, map.cend());

        xxas::assert_eq(it_1->second, 123);
        xxas::assert_eq(it_2->second, 321);
    };

    constexpr xxas::Tests bmap
    {
        find_basic,
    };
};

int main()
{
    return xxas_tests::bmap();
};
