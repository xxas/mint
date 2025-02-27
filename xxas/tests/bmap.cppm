import std;
import xxas;

namespace xxas_tests
{
    constexpr auto init()
    {
        constexpr xxas::BMap<std::string, int, 2> map
        {
            std::pair{ "123", 123 },
            std::pair{ "321",  321 },
        };

        constexpr auto it_1 = map.find("123");
        auto it_2           = map.find("321");
        auto it_3           = map.find("132");

        xxas::assert_ne(it_1, std::nullopt);
        xxas::assert_ne(it_2, std::nullopt);
        xxas::assert_eq(it_3, std::nullopt);

        xxas::assert_eq(*it_1, 123);
        xxas::assert_eq(*it_2, 321);
    };

    constexpr xxas::Tests bmap
    {
        init,
    };
};

int main()
{
    return xxas_tests::bmap();
};
