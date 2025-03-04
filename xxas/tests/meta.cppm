import std;
import xxas;

namespace xxas_tests
{
    using namespace xxas;

    constexpr auto dedup_extends()
    {
        using A = std::variant<int, float, char>;
        using B = std::variant<int, double>;

        using AB_Ext = meta::DedupExtend<A, B>;
        using AB_Exp = meta::Template<std::variant<int, float, char, double>>;

        xxas::assert_eq(AB_Ext{}, AB_Exp{});

        using C = std::variant<std::tuple<std::string, float>, float>;
        using D = std::variant<std::tuple<float, std::string>, double>;

        using CD_Ext = xxas::meta::DedupExtend_t<C, D>;
        using CD_Exp = std::variant<std::tuple<std::string, float>, float, std::tuple<float, std::string>, double>;

        xxas::assert_eq(CD_Ext{}, CD_Exp{});

        using E = std::variant<float, double>;
        using F = meta::Template<std::variant<short, unsigned long>, std::variant<int, char>>;

        using EF_Ext = meta::DedupExtend<E, F>;
        using EF_Exp = meta::Template<std::variant<float, double, short, unsigned long, int, char>>;

        xxas::assert_eq(EF_Ext{}, EF_Exp{});
    };

    constexpr xxas::Tests meta
    {
        dedup_extends,
    };
};

int main()
{
    return xxas_tests::meta();
};
