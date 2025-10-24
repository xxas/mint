import std;
import xxas;
import mint;

using namespace mint;

// Define some simple instructions.
constexpr static auto insns = arch::Insns
{
    std::pair{"mov", [](auto& dest, const auto& src) { dest = src; }},
    std::pair{"add", [](auto& dest, const std::ranges::range auto& srcs)
    {   // accumulate sources and assign to desination.
        dest = std::accumulate(std::ranges::begin(srcs), std::ranges::end(srcs), {});
    }},
    std::pair{"prntln", [](const auto& src)
    {
        std::println("{}", src);
    }},
};

// Define language keywords.
constexpr static auto registers = arch::Keywords
{   // Registers.
    {"gp0", Traits{traits::Bitness::b64, traits::Source::Register}},
    std::pair{"gp1", Traits{traits::Bitness::b64, traits::Source::Register}},
    std::pair{"gp2", Traits{traits::Bitness::b64, traits::Source::Register}},

    // Literals.
    std::pair{"dword", Traits{traits::Bitness::b64}},
    std::pair{"word",  Traits{traits::Bitness::b32}},
    std::pair{"ptr",   Traits{traits::Source::Memory}},
};

int main()
{
    constexpr auto program =
      R"(
      .word array: 1, 2, 3, 4
      .text main: mov     gp0, 0xff
                  add     gp1, gp0, ptr[array + 8]
                  prntln  gp1                         # prints 258
                  prntln  ptr[array + 8]              # prints 3
      )";
};
