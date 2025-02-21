import std;
import xxas;

namespace xxas_test
{
    enum class EnumKey
    {
        First, Second, Third
    };

    void enum_double_entries()
    {
        constexpr static xxas::BMultiMap map
        {
            xxas::entry(EnumKey::First,  { 0.0,  0.5,  0.75,  1.0 }),
            xxas::entry(EnumKey::Second, { 0.0, -0.5, -0.75, -1.0 }),
        };

        constexpr auto find_0 = map.find(EnumKey::First);
        auto           find_1 = map.find(EnumKey::Second);
        auto           find_2 = map.find(EnumKey::Third);

        xxas::assert_ne(find_0, std::nullopt);
        xxas::assert_ne(find_1, std::nullopt);
        xxas::assert_eq(find_2, std::nullopt);

        xxas::assert_eq(find_0->at(3), 1.0);
        xxas::assert_eq(find_1->size(), 4);
    };

    void string_function_entries()
    {
        using Function = int(*)(int);
        constexpr Function funct_0 = [](int A) -> int { return A * 1; };
        constexpr Function funct_1 = [](int A) -> int { return A * 2; };

        constexpr static xxas::BMultiMap map
        {
            xxas::entry("First",   { funct_0, funct_1 }),
            xxas::entry("Second",  { funct_0 }),
        };
 
        constexpr auto find_0 = map.find("First");
        auto           find_1 = map.find("Second");
        auto           find_2 = map.find("Third");

        xxas::assert_ne(find_0, std::nullopt);
        xxas::assert_ne(find_1, std::nullopt);
        xxas::assert_eq(find_2, std::nullopt);

        constexpr auto result_0 = std::invoke(find_0->at(1), 10);
        auto           result_1 = std::invoke(find_1->at(0), 20);

        xxas::assert_eq(result_0, result_1);
    };

    constexpr inline auto bmultimap = xxas::Tests
    {
        enum_double_entries, string_function_entries,
    };
};

int main()
{
    return xxas_test::bmultimap();
};
