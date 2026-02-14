export module mint: expression;

import std;
import :traits;
import :scalar;

/***
 **  module:   mint: expression
 **  purpose:  Arithmetic expression tree with owned constant values,
 **            symbol resolution, precedence handling, and evaluation.
 ***/

namespace mint
{
    namespace expr
    {
        export enum class Operator: std::uint8_t
        {   // Arithmetic.
            Add = 0, Sub, Mul, Div, Mod,

            // Bitwise.
            And, Or, Xor, Shl, Shr,

            // Unary.
            Neg, Not,
        };

        // Owned constant value in expression tree.
        export struct Constant
        {
            std::variant<std::int64_t, std::uint64_t, double> value;

            constexpr Constant(std::int64_t v)  : value{ v } {};
            constexpr Constant(std::uint64_t v) : value{ v } {};
            constexpr Constant(double v)        : value{ v } {};

            template<xxas::meta::arithmetic T>
            constexpr auto as() const
                -> T
            {
                return std::visit([](auto v) -> T
                {
                    return static_cast<T>(v);
                }, this->value);
            };

            constexpr auto is_signed() const
                -> bool
            {
                return std::holds_alternative<std::int64_t>(this->value);
            };

            constexpr auto is_floating() const
                -> bool
            {
                return std::holds_alternative<double>(this->value);
            };
        };

        // Symbol reference (register, label, variable).
        export struct Symbol
        {
            std::string_view name;

            constexpr Symbol(std::string_view name)
                : name{ name } {};
        };

        // Get operator precedence.
        constexpr auto precedence(const Operator op)
            -> std::int8_t
        {
            constexpr static std::array precedences
            {
                5,   // Add
                5,   // Sub
                6,   // Mul
                6,   // Div
                6,   // Mod
                3,   // And
                1,   // Or
                2,   // Xor
                4,   // Shl
                4,   // Shr
                7,  // Neg
                7,  // Not
            };

            return precedences[static_cast<std::size_t>(op)];
        };

        // Check if operator is unary.
        constexpr auto is_unary(const Operator op)
            -> bool
        {
            return op == Operator::Neg || op == Operator::Not;
        };

        // Operator dispatch table for integral types.
        template<class T, class U> constexpr auto visit_integral(const Operator type, T first, U second)
            -> T
            requires std::integral<T>
        {
            constexpr static std::array ops
            {
                // Binary arithmetic.
                +[](T a, U b) { return std::add_sat(a, static_cast<T>(b)); },
                +[](T a, U b) { return std::sub_sat(a, static_cast<T>(b)); },
                +[](T a, U b) { return std::mul_sat(a, static_cast<T>(b)); },
                +[](T a, U b) { return b != 0 ? std::div_sat(a, static_cast<T>(b)) : T{}; },
                +[](T a, U b) { return b != 0 ? a % static_cast<T>(b) : T{}; },

                // Binary bitwise.
                +[](T a, U b) { return a & static_cast<T>(b); },
                +[](T a, U b) { return a | static_cast<T>(b); },
                +[](T a, U b) { return a ^ static_cast<T>(b); },
                +[](T a, U b) { return a << static_cast<T>(b); },
                +[](T a, U b) { return a >> static_cast<T>(b); },

                // Unary operators (ignore second operand).
                +[](T a, U) { return -a; },
                +[](T a, U) { return ~a; },
            };

            return std::invoke(ops[static_cast<std::size_t>(type)], first, second);
        };

        // Operator dispatch table for floating point types.
        template<class T, class U> constexpr auto visit_floating(const Operator type, T first, U second)
            -> T
            requires std::floating_point<T>
        {
            constexpr static std::array ops
            {
                // Binary arithmetic.
                +[](T a, U b) { return a + static_cast<T>(b); },
                +[](T a, U b) { return a - static_cast<T>(b); },
                +[](T a, U b) { return a * static_cast<T>(b); },
                +[](T a, U b) { return b != 0 ? a / static_cast<T>(b) : T{}; },
                +[](T a, U b) { return std::fmod(a, static_cast<T>(b)); },

                // Binary bitwise (no-op for floating point).
                +[](T a, U) { return a; },
                +[](T a, U) { return a; },
                +[](T a, U) { return a; },
                +[](T a, U) { return a; },
                +[](T a, U) { return a; },

                // Unary operators (ignore second operand).
                +[](T a, U) -> T { return -a; },
                +[](T a, U) -> T { return !a; },
            };

            return std::invoke(ops[static_cast<std::size_t>(type)], first, second);
        };

        // Visit binary or unary operator.
        template<class T, class U> constexpr auto visit(const Operator type, T first, U second)
            -> T
        {
            if constexpr(std::integral<T>)
            {
                return visit_integral(type, first, second);
            }
            else
            {
                return visit_floating(type, first, second);
            };
        };

        // Visit unary operator (delegates to binary visit with dummy second operand).
        template<class T> constexpr auto visit(const Operator type, T operand)
            -> T
        {
            return visit(type, operand, T{});
        };

        // Forward declare Node for pointer usage.
        export struct Node;

        export struct Branch
        {
            using NodePtr = std::unique_ptr<Node>;

            Operator operation;
            NodePtr  left;
            NodePtr  right;

            // Constructor defined after Node is complete.
            Branch(Operator op, NodePtr left, NodePtr right);
        };

        export struct Unary
        {
            using NodePtr = std::unique_ptr<Node>;

            Operator operation;
            NodePtr  operand;

            // Constructor defined after Node is complete.
            Unary(Operator op, NodePtr operand);
        };

        export struct Memory
        {
            using NodePtr = std::unique_ptr<Node>;

            NodePtr base;    // Base address expression.
            NodePtr offset;  // Optional offset.
            NodePtr scale;   // Optional scale factor.

            // Constructors defined after Node is complete.
            Memory(NodePtr base);
            Memory(NodePtr base, NodePtr offset);
            Memory(NodePtr base, NodePtr offset, NodePtr scale);
        };

        // Expression tree node (complete type definition).
        struct Node: std::variant<Constant, Symbol, Branch, Unary, Memory>
        {
            using std::variant<Constant, Symbol, Branch, Unary, Memory>::variant;
        };

        // Define constructors now that Node is complete.
        inline Branch::Branch(Operator op, NodePtr left, NodePtr right)
            : operation{ op }, left{ std::move(left) }, right{ std::move(right) } {};

        inline Unary::Unary(Operator op, NodePtr operand)
            : operation{ op }, operand{ std::move(operand) } {};

        inline Memory::Memory(NodePtr base)
            : base{ std::move(base) }, offset{ nullptr }, scale{ nullptr } {};

        inline Memory::Memory(NodePtr base, NodePtr offset)
            : base{ std::move(base) }, offset{ std::move(offset) }, scale{ nullptr } {};

        inline Memory::Memory(NodePtr base, NodePtr offset, NodePtr scale)
            : base{ std::move(base) }, offset{ std::move(offset) }, scale{ std::move(scale) } {};

        // Token stream with operands and operators.
        export using Tokens = std::vector<std::pair<std::variant<Constant, Symbol>, Operator>>;
    };

    // Symbol resolver interface.
    export template<xxas::meta::arithmetic T>
    using SymbolResolver = std::function<std::optional<T>(std::string_view)>;

    export struct Expression
    {
        using Node      = expr::Node;
        using NodePtr   = std::unique_ptr<Node>;

        using Constant  = expr::Constant;
        using Symbol    = expr::Symbol;
        using Branch    = expr::Branch;
        using Unary     = expr::Unary;
        using Memory    = expr::Memory;

        using Tokens    = expr::Tokens;
        using Operator  = expr::Operator;

        NodePtr root;

        explicit Expression(Constant&& constant) : root{ std::make_unique<Node>(std::move(constant)) } {};
        explicit Expression(Symbol&& symbol)     : root{ std::make_unique<Node>(std::move(symbol)) } {};
        explicit Expression(Branch&& branch)     : root{ std::make_unique<Node>(std::move(branch)) } {};
        explicit Expression(Unary&& unary)       : root{ std::make_unique<Node>(std::move(unary)) } {};
        explicit Expression(Memory&& memory)     : root{ std::make_unique<Node>(std::move(memory)) } {};
        explicit Expression(NodePtr&& node)      : root{ std::move(node) } {};

        // Check if expression is a constant.
        constexpr auto constant() const
            -> std::optional<Constant>
        {
            return std::visit(xxas::meta::Overloads
            {
                [](const Constant& c) -> std::optional<Constant> { return c; },
                [](const auto&)       -> std::optional<Constant> { return std::nullopt; },
            }, *this->root);
        };

        // Check if expression is a symbol.
        constexpr auto symbol() const
            -> std::optional<std::string_view>
        {
            return std::visit(xxas::meta::Overloads
            {
                [](const Symbol& sym) -> std::optional<std::string_view> { return sym.name; },
                [](const auto&)       -> std::optional<std::string_view> { return std::nullopt; },
            }, *this->root);
        };

        // Check if expression is memory access.
        constexpr auto is_memory() const
            -> bool
        {
            return std::holds_alternative<Memory>(*this->root);
        };

        // Evaluate the expression with symbol resolver.
        template<xxas::meta::arithmetic T> constexpr auto evaluate(const SymbolResolver<T>& resolver) const
            -> std::optional<T>
        {
            // Helper function for recursive evaluation
            auto evaluate_node = [&](this const auto& self, const Node& node) -> std::optional<T>
            {
                return std::visit(xxas::meta::Overloads
                {
                    [](const Constant& constant) -> std::optional<T>
                    {
                        return constant.as<T>();
                    },
                    [&](const Symbol& sym) -> std::optional<T>
                    {
                        if(!resolver)
                        {
                            return std::nullopt;
                        };
                        return resolver(sym.name);
                    },
                    [&](const Unary& unary) -> std::optional<T>
                    {
                        const auto operand = self(*unary.operand);
                        if(!operand)
                        {
                            return std::nullopt;
                        };

                        return expr::visit(unary.operation, *operand);
                    },
                    [&](const Branch& branch) -> std::optional<T>
                    {
                        const auto left = self(*branch.left);
                        const auto right = self(*branch.right);

                        if(!left || !right)
                        {
                            return std::nullopt;
                        };

                        return expr::visit(branch.operation, *left, *right);
                    },
                    [&](const Memory& mem) -> std::optional<T>
                    {
                        auto base = self(*mem.base);
                        if(!base)
                        {
                            return std::nullopt;
                        };

                        if(mem.offset)
                        {
                            const auto offset = self(*mem.offset);
                            if(!offset)
                            {
                                return std::nullopt;
                            };
                            base = *base + *offset;
                        };

                        if(mem.scale)
                        {
                            const auto scale = self(*mem.scale);
                            if(!scale)
                            {
                                return std::nullopt;
                            };
                            base = *base * *scale;
                        };

                        return base;
                    },
                }, node);
            };

            return evaluate_node(*this->root);
        };

        // Evaluate with no symbol resolver (constants only).
        template<xxas::meta::arithmetic T> constexpr auto evaluate() const
            -> std::optional<T>
        {
            return this->evaluate<T>(nullptr);
        };

        // Simplify expression (constant folding).
        constexpr auto simplify() const
            -> Expression
        {
            auto simplify_node = [](this const auto& self, const Node& node) -> NodePtr
            {
                return std::visit(xxas::meta::Overloads
                {
                    [](const Constant& constant) -> NodePtr
                    {
                        return std::make_unique<Node>(constant);
                    },
                    [](const Symbol& sym) -> NodePtr
                    {
                        return std::make_unique<Node>(sym);
                    },
                    [&](const Unary& unary) -> NodePtr
                    {
                        auto operand = self(*unary.operand);

                        // If operand is constant, evaluate.
                        if(const auto* constant = std::get_if<Constant>(operand.get()))
                        {
                            const auto value = constant->template as<std::int64_t>();
                            const auto result = expr::visit(unary.operation, value);
                            return std::make_unique<Node>(Constant{ result });
                        };

                        return std::make_unique<Node>(Unary{ unary.operation, std::move(operand) });
                    },
                    [&](const Branch& branch) -> NodePtr
                    {
                        auto left = self(*branch.left);
                        auto right = self(*branch.right);

                        // If both sides are constants, evaluate.
                        if(const auto* left_constant = std::get_if<Constant>(left.get()))
                        {
                            if(const auto* right_constant = std::get_if<Constant>(right.get()))
                            {
                                const auto left_val = left_constant->template as<std::int64_t>();
                                const auto right_val = right_constant->template as<std::int64_t>();
                                const auto result = expr::visit(branch.operation, left_val, right_val);

                                return std::make_unique<Node>(Constant{ result });
                            };
                        };

                        return std::make_unique<Node>(Branch
                        {
                            branch.operation,
                            std::move(left),
                            std::move(right)
                        });
                    },
                    [&](const Memory& mem) -> NodePtr
                    {
                        auto base = self(*mem.base);
                        auto offset = mem.offset ? self(*mem.offset) : nullptr;
                        auto scale = mem.scale ? self(*mem.scale) : nullptr;

                        return std::make_unique<Node>(Memory
                        {
                            std::move(base),
                            std::move(offset),
                            std::move(scale)
                        });
                    },
                }, node);
            };

            return Expression{ simplify_node(*this->root) };
        };

        enum class ParseErr: std::uint8_t
        {
            Empty,
            InvalidOperator,
            MismatchedParentheses,
        };

        using ParseResult = xxas::Result<Expression, ParseErr>;

        // Parse tokens into expression tree with parenthesis support.
        static auto parse(const Tokens& tokens)
            -> ParseResult
        {
            if(tokens.empty())
            {
                return xxas::error(ParseErr::Empty, "Expression was passed an empty range of tokens");
            };

            std::vector<NodePtr>  nodes{};
            std::vector<Operator> operators{};

            // Add first operand.
            std::visit([&](const auto& operand)
            {
                using T = std::decay_t<decltype(operand)>;

                if constexpr(std::same_as<T, Constant>)
                {
                    nodes.push_back(std::make_unique<Node>(operand));
                }
                else if constexpr(std::same_as<T, Symbol>)
                {
                    nodes.push_back(std::make_unique<Node>(operand));
                };
            }, tokens.front().first);

            // Process remaining tokens.
            for(const auto& [operand, op]: std::ranges::subrange(tokens.begin() + 1, tokens.end()))
            {
                // Handle operator precedence.
                while(!operators.empty() && expr::precedence(operators.back()) >= expr::precedence(op))
                {
                    const auto curr_op = operators.back();
                    operators.pop_back();

                    if(expr::is_unary(curr_op))
                    {
                        auto operand_node = std::move(nodes.back());
                        nodes.pop_back();

                        nodes.push_back(std::make_unique<Node>(Unary
                        {
                            curr_op,
                            std::move(operand_node)
                        }));
                    }
                    else
                    {
                        auto right = std::move(nodes.back());
                        nodes.pop_back();

                        auto left = std::move(nodes.back());
                        nodes.pop_back();

                        nodes.push_back(std::make_unique<Node>(Branch
                        {
                            curr_op,
                            std::move(left),
                            std::move(right)
                        }));
                    };
                };

                // Add operand.
                std::visit([&](const auto& op_value)
                {
                    using T = std::decay_t<decltype(op_value)>;
                    if constexpr(std::same_as<T, Constant>)
                    {
                        nodes.push_back(std::make_unique<Node>(op_value));
                    }
                    else if constexpr(std::same_as<T, Symbol>)
                    {
                        nodes.push_back(std::make_unique<Node>(op_value));
                    };
                }, operand);

                operators.push_back(op);
            };

            // Process remaining operators.
            while(!operators.empty())
            {
                const auto curr_op = operators.back();
                operators.pop_back();

                if(expr::is_unary(curr_op))
                {
                    auto operand = std::move(nodes.back());
                    nodes.pop_back();

                    nodes.push_back(std::make_unique<Node>(Unary
                    {
                        curr_op,
                        std::move(operand)
                    }));
                }
                else
                {
                    auto right = std::move(nodes.back());
                    nodes.pop_back();
                    auto left = std::move(nodes.back());
                    nodes.pop_back();

                    nodes.push_back(std::make_unique<Node>(Branch
                    {
                        curr_op,
                        std::move(left),
                        std::move(right)
                    }));
                };
            };

            return Expression{ std::move(nodes.back()) };
        };

        static auto constant(std::int64_t value)
            -> Expression
        {
            return Expression{ Constant{ value } };
        };

        static auto constant(std::uint64_t value)
            -> Expression
        {
            return Expression{ Constant{ value } };
        };

        static auto constant(double value)
            -> Expression
        {
            return Expression{ Constant{ value } };
        };

        static auto symbol(std::string_view name)
            -> Expression
        {
            return Expression{ Symbol{ name } };
        };

        static auto memory(Expression&& base)
            -> Expression
        {
            return Expression{ Memory{ std::move(base.root) } };
        };

        static auto memory(Expression&& base, Expression&& offset)
            -> Expression
        {
            return Expression{ Memory{ std::move(base.root), std::move(offset.root) } };
        };

        static auto unary(Operator op, Expression&& operand)
            -> Expression
        {
            return Expression{ Unary{ op, std::move(operand.root) } };
        };

        static auto binary(Operator op, Expression&& left, Expression&& right)
            -> Expression
        {
            return Expression{ Branch{ op, std::move(left.root), std::move(right.root) } };
        };
    };
};
