import std;
import xxas;
import mint;

namespace mint_test
{
    using namespace mint;

    constexpr void bindings()
    {
        std::uint32_t ui32 = 100;
        std::string   str8 = "hello world!";
        float         fl32  = std::numbers::pi_v<float>;

        std::vector<std::span<std::byte>> spans
        {
            {reinterpret_cast<std::byte*>(&ui32), sizeof(std::uint32_t)},
            {reinterpret_cast<std::byte*>(&str8), sizeof(std::string)},
            {reinterpret_cast<std::byte*>(&fl32), sizeof(float)}
        };

        std::function fn = [](std::uint32_t& a, std::string& b, float& c)
            -> Binding::FunctResult
        {
            xxas::assert_eq(a, 100);
            xxas::assert_eq(b, std::string("hello world!"));
            xxas::assert_eq(c, std::numbers::pi_v<float>);

            std::println("{} {} {}", b, a, c);

            // Assign a new value to a.
            a = 300;
            c = std::numbers::egamma_v<float>;

            return {};
        };

        // invoke the test binding.
        if(auto binding = Binding::create(fn, spans); binding)
        {
            auto result = std::invoke(*binding);

            // Assert that the result is a value.
            xxas::assert_eq(result.has_value(), true);
        };

        // Ensure the original value was modified by the lambda.
        xxas::assert_eq(ui32, 300);
        xxas::assert_eq(fl32, std::numbers::egamma_v<float>);
    };

    constexpr inline xxas::Tests execution
    {
        bindings,
    };
};

int main()
{
    return mint_test::execution();
};
