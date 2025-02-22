export module mint: expression;

import std;

import :traits;
import :scalar;

/*** **
 **
 **  module:   mint: expression
 **  purpose:  Arithmetic expression tree with scalar operands,
 **            precendence handling, parsing, and evaluation.
 **
 *** **/

namespace mint
{
    namespace expr
    {
        export enum class Operator: std::uint8_t
        {
            Add = 0, Sub, Mul, Div,
        };

        template<class T, class U> constexpr auto visit(Operator type, const T& a, const U& b)
            -> T
        {
            constexpr static std::array map
            {
                +[](T a, U b) constexpr -> T { return a + b; },
                +[](T a, U b) constexpr -> T { return a - b; },
                +[](T a, U b) constexpr -> T { return a * b; },
                +[](T a, U b) constexpr -> T { return a / b; },
            };

            return map[static_cast<std::size_t>(type)](a, b);
        };

        export using Tokens = std::vector<std::pair<Scalar, Operator>>;
    };

    export struct Expression
    {
        using ExprPtr = std::unique_ptr<Expression>;

        struct Operation
        {
            expr::Operator op;
            ExprPtr        left;
            ExprPtr        right;

            Operation(expr::Operator o, ExprPtr l, ExprPtr r)
                : op(o), left(std::move(l)), right(std::move(r)) {};
        };

        using Variant = std::variant<Scalar, Operation>;

        // Variant between a and a constant.
        Variant expr;

        // Construct a constant.
        explicit Expression(Scalar value)
            : expr(std::move(value)) {};

        // Construct a binary operation.
        Expression(expr::Operator op, ExprPtr left, ExprPtr right)
            : expr(Operation(op, std::move(left), std::move(right))) {};

        template <class T> constexpr auto evaluate() const
            -> T
        {
            struct Evaluator
            {
                constexpr auto operator()(const Scalar& scalar) const
                    -> T
                {   // Return the scalar as T.
                    return scalar.template as<T>();
                };

                constexpr auto operator()(const Operation& binary) const
                    -> T
                {   // Extract the left and right operands.
                    T left  = std::visit(*this, binary.left->expr);
                    T right = std::visit(*this, binary.right->expr);

                    // Invoke the binary operator.
                    return expr::visit(binary.op, left, right);
                };
            };

            // Recursively evaluate each operator and operands.
            return std::visit(Evaluator{}, expr);
        };

        enum class ParseErrs: std::uint8_t
        {
            Empty,
        };

        using ParseErr    = xxas::Error<ParseErrs>;
        using ParseResult = std::expected<ExprPtr, ParseErr>;

        // Shunting-yard parse algorithm implementation. This takes a range of tokens--represented by
        // operands with operators between, or in this case after.
        static auto parse(const expr::Tokens& tokens)
            -> ParseResult
        {
            if (tokens.empty())
            {
                return ParseErr::err(ParseErrs::Empty, "Expression was passed an empty range of tokens");
            };

            // Returns the precedence of a binary operator.
            auto get_precedence = [](expr::Operator type)
            {
                switch(type)
                {
                    case expr::Operator::Mul:
                    case expr::Operator::Div:
                    {
                        return 2;
                    };
                    case expr::Operator::Add:
                    case expr::Operator::Sub:
                    {
                        return 1;
                    };
                    default:
                    {
                        return 0;
                    };
                };
            };

            std::vector<Expression::ExprPtr> values;
            std::vector<expr::Operator>      operators;

            values.push_back(std::make_unique<Expression>(tokens.front().first));

            for(auto& [scalar, op]: std::ranges::subrange(tokens.begin() + 1uz, tokens.end()))
            {
                while(!operators.empty() && get_precedence(operators.back()) >= get_precedence(op))
                {
                    auto right = std::move(values.back()); values.pop_back();
                    auto left  = std::move(values.back()); values.pop_back();

                    values.push_back(std::make_unique<Expression>(operators.back(), std::move(left), std::move(right)));
                    operators.pop_back();
                };

                values.push_back(std::make_unique<Expression>(scalar));
                operators.push_back(op);
            };

            while(!operators.empty())
            {
                auto right = std::move(values.back()); values.pop_back();
                auto left  = std::move(values.back()); values.pop_back();

                values.push_back(std::make_unique<Expression>(operators.back(), std::move(left), std::move(right)));
                operators.pop_back();
            };

            return std::move(values.back());
        };
    };
};
