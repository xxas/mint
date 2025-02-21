import std;
import xxas;
import mint;

namespace mint_test
{
    using namespace mint;

    constexpr Traits src_imm_i64 =
    {
        .direction = traits::Direction::Src,
        .sources   = traits::Source::Immediate,
        .bitness   = traits::Bitness::b64,
        .format    = traits::Format::Integral,
    };

    constexpr Traits src_imm_f64 =
    {
        .direction = traits::Direction::Src,
        .sources   = traits::Source::Immediate,
        .bitness   = traits::Bitness::b64,
        .format    = traits::Format::Floating,
    };

    
};

int main()
{
    return 0;
};
