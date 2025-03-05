import std;
import xxas;

namespace xxas_tests
{
    using namespace xxas;

    constexpr auto dedup_extends()
    {   // Define A and B templates with basic arithmetic types.
        using A = std::tuple<int, float, char>;
        using B = std::tuple<int, double>;

        // Extend A by B, deduplicate.
        using AB_Ext = meta::DedupExtend<A, B>;
        using AB_Exp = meta::Template<std::tuple<int, float, char, double>>;

        xxas::assert_eq(AB_Ext{}, AB_Exp{});

        // Define C and D with mixed object types and arithmetic types.
        using C = std::variant<std::tuple<std::string, float>, float>;
        using D = std::variant<std::tuple<float, std::string>, double>;

        // Extend and deduplicate.
        using CD_Ext = meta::DedupExtend<C, D>;
        using CD_Exp = meta::Template<std::variant<std::tuple<std::string, float>, float, std::tuple<float, std::string>, double>>;

        xxas::assert_eq(CD_Ext{}, CD_Exp{});

        // Define E and F using a meta::Template that contains multiple object types that should be folded.
        using E = std::variant<float, double>;
        using F = meta::Template<std::variant<short, unsigned long>, std::variant<int, char>>;

        // Extend, deduplicate and attempt an index.
        using EF_Ext = meta::DedupExtend<E, F>;
        using EF_Exp = meta::Template<std::variant<float, double, short, unsigned long, int, char>>;
        using EF_2   = meta::Template<meta::Element_t<EF_Ext::Type, 2>>;

        xxas::assert_eq(EF_Ext{}, EF_Exp{});
        xxas::assert_eq(EF_2{}, meta::Template<short>{});
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
