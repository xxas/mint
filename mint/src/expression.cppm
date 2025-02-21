export module mint: expression;

import std;

import :traits;
import :scalar;

/*** **
 **
 **  module:   xxas: expression
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

        export struct Operation
        {
            Operator type;

            template <class T, class U> using OpFunc = T (*)(T, U);
            template <class T, class U> constexpr static inline std::array<OpFunc<T, U>, 4> map
            {
                +[](T a, U b) constexpr -> T { return a + b; },
                +[](T a, U b) constexpr -> T { return a - b; },
                +[](T a, U b) constexpr -> T { return a * b; },
                +[](T a, U b) constexpr -> T { return a / b; },
            };

            template<class T, class U> constexpr auto operator()(const T& a, const U& b) const
                -> T
            {
                return map<T, U>[static_cast<std::size_t>(this->type)](a, b);
            };
        };
    };

    export struct Expression
    {
        using ExprPtr = std::unique_ptr<Expression>;

        struct Node
        {
            Scalar value;
            explicit Node(Scalar v) : value(std::move(v)) {};
        };

        struct BinaryOp
        {
            expr::Operation op;
            ExprPtr left;
            ExprPtr right;

            BinaryOp(expr::Operation o, ExprPtr l, ExprPtr r)
                : op(o), left(std::move(l)), right(std::move(r)) {};
        };

        std::variant<Node, BinaryOp> expr;

        explicit Expression(Scalar value) : expr(Node(std::move(value))) {};

        Expression(expr::Operation op, ExprPtr left, ExprPtr right)
            : expr(BinaryOp(op, std::move(left), std::move(right))) {};

        template <class T> constexpr auto evaluate() const -> T
        {
            struct Evaluator
            {
                constexpr auto operator()(const Node& node) const
                    -> T
                {
                    return node.value.template as<T>();
                }

                constexpr auto operator()(const BinaryOp& binop) const
                    -> T
                {
                    T left = std::visit(*this, binop.left->expr);
                    T right = std::visit(*this, binop.right->expr);
                    return std::invoke(binop.op, left, right);
                }
            };

            return std::visit(Evaluator{}, expr);
        };

        static auto parse(std::vector<std::pair<Scalar, expr::Operation>> tokens)
            -> ExprPtr
        {
            if (tokens.empty())
            {
                return nullptr;
            };

            auto get_precedence = [](expr::Operator type)
            {
                switch(type)
                {
                    case expr::Operator::Mul:
                    case expr::Operator::Div:
                        return 2;
                    case expr::Operator::Add:
                    case expr::Operator::Sub:
                        return 1;
                    default:
                        return 0;
                }
            };

            std::vector<Expression::ExprPtr> values;
            std::vector<expr::Operation> operators;

            values.push_back(std::make_unique<Expression>(tokens[0].first));

            for (std::size_t i = 1; i < tokens.size(); i++)
            {
                expr::Operation op = tokens[i].second;
                Scalar next_value = tokens[i].first;

                while (!operators.empty() && get_precedence(operators.back().type) >= get_precedence(op.type))
                {
                    auto right = std::move(values.back()); values.pop_back();
                    auto left = std::move(values.back()); values.pop_back();

                    values.push_back(std::make_unique<Expression>(operators.back(), std::move(left), std::move(right)));
                    operators.pop_back();
                };

                values.push_back(std::make_unique<Expression>(next_value));
                operators.push_back(op);
            };

            while (!operators.empty())
            {
                auto right = std::move(values.back()); values.pop_back();
                auto left = std::move(values.back()); values.pop_back();

                values.push_back(std::make_unique<Expression>(operators.back(), std::move(left), std::move(right)));
                operators.pop_back();
            };

            return std::move(values.back());
        };
    };
};
