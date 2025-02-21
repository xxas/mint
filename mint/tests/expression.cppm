import std;
import xxas;
import mint;

namespace mint_test
{
    using namespace mint;

    constexpr void pointer_arithmetic()
    {
        double array[] = {100.0, 200.0, 300.0, 400.0, 500.0};

        std::uintptr_t base_address = reinterpret_cast<std::uintptr_t>(array);
        std::uintptr_t index = 3;
        std::uintptr_t align = sizeof(double);

        std::vector<std::pair<Scalar, expr::Operation>> tokens
        {
            {Scalar::from(base_address), {}},
            {Scalar::from(index),        {expr::Operator::Add}},
            {Scalar::from(align),        {expr::Operator::Mul}},
        };

        auto expression = Expression::parse(tokens);
        auto result     = expression->evaluate<std::uintptr_t>();

        auto value = *reinterpret_cast<double*>(result);
        xxas::assert_eq(value, 400.0);
    };

    constexpr inline xxas::Tests execution
    {
        pointer_arithmetic,
    };
};

int main()
{
    return mint_test::execution();
};
