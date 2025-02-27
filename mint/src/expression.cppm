export module mint: expression;

import std;
import :traits;
import :scalar;

/***
 **  module:   mint: expression
 **  purpose:  Arithmetic expression tree with scalar operands,
 **            precedence handling, parsing, and evaluation.
 ***/

namespace mint
{
    namespace expr
    {
        export enum class Operator : std::uint8_t
        {
            Add = 0, Sub, Mul, Div,
        };

        // Visit binary operator with operands first and second.
        template<class T, class U> constexpr auto visit(Operator type, T first, U second)
            -> T
        {
            constexpr static std::array map
            {
                []
                {
                    if constexpr(std::integral<T>)
                    {   // For integral types: Use saturated methods to prevent
                        // underflow or overflow errors.
                        return std::array
                        {
                            +std::add_sat<T>,
                            +std::sub_sat<T>,
                            +std::mul_sat<T>,
                            +std::div_sat<T>,
                        };
                    };
                    return std::array
                    {   // For any other type use traditional operators.
                        +[](T a, U b) noexcept { return a + b; },
                        +[](T a, U b) noexcept { return a - b; },
                        +[](T a, U b) noexcept { return a * b; },
                        +[](T a, U b) noexcept { return a / b; },
                    };
                }()
            };

            // Extract the binary operation for the type.
            auto& funct =  map[static_cast<std::size_t>(type)];

            // Invoke with the operands.
            return std::invoke(funct, first, second);
        };

        export struct Node;
        export struct Branch
        {
            using NodePtr = std::unique_ptr<Node>;

            Operator operation;
            NodePtr  left;
            NodePtr  right;
        };

        export using Leaf   = Scalar;
        struct Node: std::variant<Leaf, Branch>
        {
            using std::variant<Leaf, Branch>::variant;
        };

        export using Tokens = std::vector<std::pair<Scalar, Operator>>;
    };

    export struct Expression
    {
        using Node     = expr::Node;
        using NodePtr  = std::unique_ptr<Node>;

        using Leaf     = expr::Leaf;
        using Branch   = expr::Branch;

        using Tokens   = expr::Tokens;
        using Operator = expr::Operator;

        NodePtr root;

        explicit Expression(Leaf&& leaf)
            : root(std::make_unique<Node>(std::move(leaf))) {};

        explicit Expression(Branch&& branch)
            : root(std::make_unique<Node>(std::move(branch))) {};

        explicit Expression(NodePtr&& node)
            : root(std::move(node)) {};

         constexpr auto constant() const
            -> std::optional<Leaf>
        {
            return this->root->visit(xxas::meta::Overloads
            {
                [](const expr::Leaf& leaf)     -> std::optional<Scalar> { return std::make_optional(leaf); },
                [](const expr::Branch& branch) -> std::optional<Scalar> { return std::nullopt; },
            });
        };

        // Evaluate the expression interpreting scalars as T.
        template<xxas::meta::arithmetic T> constexpr auto evaluate() const
            -> T
        {
            constexpr auto evaluate_leaf = [](const Leaf& leaf)
                -> T
            {
                return leaf.as<T>();
            };

            constexpr auto evaluate_branch = [](this const auto& self, const Branch& branch)
                -> T
            {   // Evaluate the left and right expressions.
                T left  = branch.left->visit(self);
                T right = branch.right->visit(self);

                // Visit the branch operation and evaluate the current expression.
                return expr::visit(branch.operation, left, right);
            };

            // Recusively visit each expression in the tree starting from the root.
            return this->root->visit(xxas::meta::Overloads
            {
                evaluate_leaf, evaluate_branch,
            });
        };

        enum class ParseErrs : std::uint8_t
        {
            Empty,
        };

        using ParseErr    = xxas::Error<ParseErrs>;
        using ParseResult = std::expected<Expression, ParseErr>;

        static auto parse(const Tokens& tokens)
            -> ParseResult
        {
            if (tokens.empty())
            {
                return ParseErr::err(ParseErrs::Empty, "Expression was passed an empty range of tokens");
            };

            auto get_precedence = [](Operator type)
                -> std::int8_t
            {
                switch(type)
                {
                    case expr::Operator::Mul:
                    case expr::Operator::Div:
                    {
                        return 5;
                    };
                    case expr::Operator::Add:
                    case expr::Operator::Sub:
                    {
                        return 4;
                    };
                    default:
                    {
                        return -1;
                    };
                };
            };

            std::vector<NodePtr>  nodes;
            std::vector<Operator> operators;

            nodes.push_back(std::make_unique<Node>(tokens.front().first));

            for(auto& [scalar, op]: std::ranges::subrange(tokens.begin() + 1u, tokens.end()))
            {
                while (!operators.empty() && get_precedence(operators.back()) >= get_precedence(op))
                {
                    auto right = std::move(nodes.back()); nodes.pop_back();
                    auto left  = std::move(nodes.back()); nodes.pop_back();

                    nodes.push_back(std::make_unique<Node>(
                          Branch
                          {
                              operators.back(), std::move(left), std::move(right)
                          }
                    ));

                    operators.pop_back();
                };

                nodes.push_back(std::make_unique<Node>(scalar));
                operators.push_back(op);
            };

            while(!operators.empty())
            {
                auto right = std::move(nodes.back()); nodes.pop_back();
                auto left  = std::move(nodes.back()); nodes.pop_back();

                nodes.push_back(std::make_unique<Node>(
                      Branch
                      {
                          operators.back(), std::move(left), std::move(right)
                      }
                ));

                operators.pop_back();
            };

            return Expression
            {
                std::move(nodes.back())
            };
        };
    };
};
