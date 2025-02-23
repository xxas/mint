export module mint:expression;
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
            Add = 0, Sub, Mul, Div
        };

        // Visit binary operator with operands first and second.
        template <class T, class U> constexpr auto visit(Operator type, T first, U second)
            -> T
        {
            constexpr static std::array map
            {
                +[](T first, U second) constexpr -> T { return first + second; },
                +[](T first, U second) constexpr -> T { return first - second; },
                +[](T first, U second) constexpr -> T { return first * second; },
                +[](T first, U second) constexpr -> T { return first / second; },
            };

            // Extract the binary operation for the type.
            auto& funct =  map[static_cast<std::size_t>(type)];

            // Invoke with the operands.
            return std::invoke(funct, first, second);
        }

        struct Node;
        struct Branch
        {
            using NodePtr = std::unique_ptr<Node>;
            Operator operation;
            NodePtr  left;
            NodePtr  right;
        };

        struct Node
        {
            using Leaf    = Scalar;
            using Variant = std::variant<Branch, Leaf>;
            Variant variant;

            explicit Node(Leaf leaf) : variant(std::move(leaf)) {};
            explicit Node(Branch branch) : variant(std::move(branch)) {};
        };

        export using Tokens = std::vector<std::pair<Scalar, Operator>>;
    };

    export struct Expression
    {
        using NodePtr = std::unique_ptr<expr::Node>;

        NodePtr root;

        explicit Expression(expr::Node::Leaf leaf)
            : root(std::make_unique<expr::Node>(std::move(leaf))) {};

        explicit Expression(expr::Branch branch)
            : root(std::make_unique<expr::Node>(std::move(branch))) {};

        explicit Expression(NodePtr node)
            : root(std::move(node)) {};

         constexpr auto constant() const
            -> std::optional<expr::Node::Leaf>
        {
            return this->root->variant.visit(xxas::meta::Overloads
            {
                [](const expr::Node::Leaf& leaf) -> std::optional<Scalar> { return std::make_optional(leaf); },
                [](const expr::Branch& branch)   -> std::optional<Scalar> { return std::nullopt; }
            });
        };

        // Evaluate the expression interpreting scalars as T.
        template<class T> constexpr auto evaluate() const
            -> T
        {
            struct Evaluator
            {
                constexpr auto operator()(const expr::Node::Leaf& leaf) const
                    -> T
                {
                    return leaf.template as<T>();
                }

                constexpr auto operator()(const expr::Branch& branch) const
                    -> T
                {
                    T left  = std::visit(*this, branch.left->variant);
                    T right = std::visit(*this, branch.right->variant);

                    return expr::visit(branch.operation, left, right);
                }
            };

            // Recusively visit each node starting from root.
            return std::visit(Evaluator{}, this->root->variant);
        };

        enum class ParseErrs : std::uint8_t
        {
            Empty,
        };

        using ParseErr    = xxas::Error<ParseErrs>;
        using ParseResult = std::expected<Expression, ParseErr>;

        static auto parse(const expr::Tokens& tokens)
            -> ParseResult
        {
            if (tokens.empty())
            {
                return ParseErr::err(ParseErrs::Empty, "Expression was passed an empty range of tokens");
            }

            auto get_precedence = [](expr::Operator type) -> int
            {
                switch (type)
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

            std::vector<NodePtr>        nodes;
            std::vector<expr::Operator> operators;

            nodes.push_back(std::make_unique<expr::Node>(tokens.front().first));

            for(auto& [scalar, op]: std::ranges::subrange(tokens.begin() + 1u, tokens.end()))
            {
                while (!operators.empty() && get_precedence(operators.back()) >= get_precedence(op))
                {
                    auto right = std::move(nodes.back()); nodes.pop_back();
                    auto left  = std::move(nodes.back()); nodes.pop_back();

                    nodes.push_back(std::make_unique<expr::Node>(
                          expr::Branch
                          {
                              operators.back(), std::move(left), std::move(right)
                          }
                    ));

                    operators.pop_back();
                };

                nodes.push_back(std::make_unique<expr::Node>(scalar));
                operators.push_back(op);
            };

            while(!operators.empty())
            {
                auto right = std::move(nodes.back()); nodes.pop_back();
                auto left  = std::move(nodes.back()); nodes.pop_back();

                nodes.push_back(std::make_unique<expr::Node>(
                      expr::Branch
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
