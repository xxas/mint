import std;
import xxas;
import mint;

namespace mint_tests
{
    using namespace mint;

    constexpr Traits src_imm_i64 =
    {
        traits::Direction::Src,
        traits::Source::Immediate,
        traits::Bitness::b64,
        traits::Format::Integral,
    };

    constexpr Traits src_imm_f64 =
    {
        traits::Direction::Src,
        traits::Source::Immediate,
        traits::Bitness::b64,
        traits::Format::Floating,
    };

    constexpr auto definition()
    {
      xxas::assert_eq(src_imm_f64.get_as<traits::Source>(), traits::Source::Immediate);
    };

    constexpr xxas::Tests semantics
    {
        definition
    };
};

int main()
{
    return mint_tests::semantics();
};
