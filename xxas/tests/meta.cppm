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

    constexpr auto forward_alike()
    {
        std::tuple floats_ints = {3.1415, 1u, 2u, 3u, 6, 9, 89.0f, 130.0515f};
        auto      accumulate = [](auto&&... ints)
        {
            return (0 + ... + ints);
        };

        // Accumulate all integer types together.
        auto sums_0 = meta::apply_alike<unsigned int, int>(accumulate, std::forward<decltype(floats_ints)>(floats_ints));
        xxas::assert_eq(sums_0, 1u + 2u + 3u + 6 + 9);

        // Accumulate all floating point types together.
        auto sums_1 = meta::apply_alike<double, float>(accumulate, std::forward<decltype(floats_ints)>(floats_ints));
        xxas::assert_eq(sums_1, 3.1415 + 89.0f + 130.0515f);

        std::tuple floats_flags = {0.15f, 0b11000000, 0b00001111, 0b00110000, 3.1415f};
        auto bit_or = [](auto&&... bits)
        {
            return (... | bits);
        };

        auto bits = meta::apply_alike<int>(bit_or, std::forward<decltype(floats_flags)>(floats_flags));
        xxas::assert_eq(bits, 0b11111111);
    };

    constexpr xxas::Tests meta
    {
        dedup_extends, forward_alike
    };
};

int main()
{
    return xxas_tests::meta();
};
