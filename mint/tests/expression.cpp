import std;
import xxas;
import mint;

namespace mint_test
{
    using namespace mint;

    // Kinetic energy formula with some arbitrary extended operations.
    // This relies heavily on the correct order of operations.
    constexpr void operator_precedence()
    {
        double pi       = std::numbers::pi_v<double>;
        double mass     = 3.0;
        double velocity = 4.0;
        double half     = 0.5;
        double ten      = 10.0;
        double five     = 5.0;

        expr::Tokens tokens
        {
            {expr::Constant(half), {}},
            {expr::Constant(mass),      expr::Operator::Mul},
            {expr::Constant(velocity),  expr::Operator::Mul},
            {expr::Constant(velocity),  expr::Operator::Mul},
            {expr::Constant(ten),       expr::Operator::Add},
            {expr::Constant(five),      expr::Operator::Sub},
            {expr::Constant(pi),        expr::Operator::Div},
        };
 
        // Parse the tokens and check the result.
        auto expression = Expression::parse(tokens);
        xxas::assert_eq(expression.has_value(), true);
 
        // Evaluate and cast the expression result to double.
        auto result = *expression->evaluate<double>();
 
        // (0.5 * mass * velocity * velocity) + 10 - (5.0 / pi);
        double expected = (half * mass * velocity * velocity) + ten - (five / pi);
 
        // Assert the result matches expectation.
        xxas::assert_eq(result, expected);
    };

    constexpr void pointer_arithmetic()
    {
        std::array<double, 5> array{100.0, 200.0, 300.0, 400.0, 500.0};

        std::uintptr_t base_address = reinterpret_cast<std::uintptr_t>(array.data());
        std::uintptr_t index        = 3;
        std::uintptr_t align        = sizeof(double);

        expr::Tokens tokens
        {
            {expr::Constant(base_address), {}},
            {expr::Constant(index),        expr::Operator::Add},
            {expr::Constant(align),        expr::Operator::Mul},
        };

        // Parse the tokens and check the result.
        auto expression = Expression::parse(tokens);
        xxas::assert_eq(expression.has_value(), true);

        // Evaluate and cast the expression result to a double,
        auto result     = expression->evaluate<std::uintptr_t>();

        xxas::assert(result.has_value(), "result.has_value()");

        auto value      = *reinterpret_cast<double*>(result.value());

        // Assert the evaluation is equal to the expected value at index.
        xxas::assert_eq(value, array[index]);
    };

    constexpr inline xxas::Tests expression
    {
        pointer_arithmetic, operator_precedence
    };
};

int main()
{
    return mint_test::expression();
};
