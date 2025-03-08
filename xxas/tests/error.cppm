import std;
import xxas;

namespace xxas_tests
{
    enum class AErr: std::uint8_t
    {
        Problem = 13,
    };


    using AError  = xxas::Error<AErr>;
    using AResult = xxas::Result<int, AErr>;

    enum class BErr: std::uint16_t
    {
        Problem = 7,
    };

    using BError  = xxas::Error<AErr, BErr>;
    using BResult = xxas::Result<int, BErr, AErr>;

    constexpr auto error_init()
    {   // Construct an error from another error.
        AError aerr{AErr::Problem, "AError problem!"};
        BError berr{std::move(aerr)};

        xxas::assert_eq(std::holds_alternative<AErr>(berr.type), true);
    };

    constexpr auto result_init()
    {   // Construct a result, and propagate an error from one.
        constexpr auto funct_a = []
            -> AResult
        {
            return AError
            {
                AErr::Problem, "AError problem!",
            };
        };

        constexpr auto funct_b = [funct_a]
            -> BResult
        {
            if(auto result = funct_a(); result)
            {
                return *result;
            }
            else
            {
                return result.error();
            };
        };

        constexpr auto result_b = funct_b();

        xxas::assert_eq(result_b.has_value(), false);
        xxas::assert_eq(std::holds_alternative<AErr>(result_b.error().type), true);
    };
 
    constexpr inline auto error = xxas::Tests
    {
        error_init, result_init
    };
};

int main()
{
    return xxas_tests::error();
};
