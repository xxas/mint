export module mint: parser;

import std;
import xxas;

import :lexer;
import :ir;
import :arch;
import :operand;
import :expression;
import :traits;
import :semantics;

/***
 **  module:   mint: parser
 **  purpose:  Transforms a token stream into an IR program.
 **            Handles operator precedence, operand trait resolution,
 **            literal parsing, and data directive recognition.
 **
 **  pipeline: Lexer -> [Parser] -> ir::Program
 ***/

namespace mint
{   // Parser: Tokens -> IR.
    export template<const auto& arch> struct Parser
    {
        using Tokens    = std::vector<lexer::Token>;
        using TokenIter = Tokens::const_iterator;
        using Operator  = Expression::Operator;

        TokenIter current;
        TokenIter end;

        // Peek current token.
        constexpr auto peek() const
            -> const lexer::Token*
        {
            if(this->current >= this->end)
            {
                return nullptr;
            };
            return &(*this->current);
        };

        // Advance to next token.
        constexpr auto advance()
            -> void
        {
            if(this->current < this->end)
            {
                ++this->current;
            };
        };

        // Skip newlines and comments.
        constexpr auto skip_noise()
            -> void
        {
            while(this->peek())
            {
                const auto type = this->peek()->type;
                if(type == lexer::TokenType::Newline || type == lexer::TokenType::Comment)
                {
                    this->advance();
                }
                else
                {
                    break;
                };
            };
        };

        // Expect specific token type.
        constexpr auto expect(lexer::TokenType type)
            -> std::optional<lexer::Token>
        {
            if(!this->peek() || this->peek()->type != type)
            {
                return std::nullopt;
            };

            const auto token = *this->peek();
            this->advance();
            return token;
        };

        // Parse literal value.
        constexpr auto parse_literal(std::string_view str)
            -> std::int64_t
        {
            std::int64_t value = 0;
            bool is_negative = false;

            // Check for negative sign.
            if(str.starts_with('-'))
            {
                is_negative = true;
                str = str.substr(1);
            };

            // Remove underscores.
            std::string cleaned;
            for(char c: str)
            {
                if(c != '_')
                {
                    cleaned += c;
                };
            };

            // Parse hex.
            if(cleaned.starts_with("0x") || cleaned.starts_with("0X"))
            {
                std::uint64_t uval = 0;
                std::from_chars(cleaned.data() + 2, cleaned.data() + cleaned.size(), uval, 16);
                value = static_cast<std::int64_t>(uval);
            }
            // Parse binary.
            else if(cleaned.starts_with("0b") || cleaned.starts_with("0B"))
            {
                std::uint64_t uval = 0;
                std::from_chars(cleaned.data() + 2, cleaned.data() + cleaned.size(), uval, 2);
                value = static_cast<std::int64_t>(uval);
            }
            // Parse octal.
            else if(cleaned.starts_with("0o") || cleaned.starts_with("0O"))
            {
                std::uint64_t uval = 0;
                std::from_chars(cleaned.data() + 2, cleaned.data() + cleaned.size(), uval, 8);
                value = static_cast<std::int64_t>(uval);
            }
            // Parse decimal.
            else
            {
                std::from_chars(cleaned.data(), cleaned.data() + cleaned.size(), value, 10);
            };

            return is_negative ? -value : value;
        };

        // Map token to operator.
        constexpr auto token_to_operator(std::string_view token)
            -> std::optional<Operator>
        {
            if(token == "+")   return Operator::Add;
            if(token == "-")   return Operator::Sub;
            if(token == "*")   return Operator::Mul;
            if(token == "/")   return Operator::Div;
            if(token == "%")   return Operator::Mod;
            if(token == "&")   return Operator::And;
            if(token == "|")   return Operator::Or;
            if(token == "^")   return Operator::Xor;
            if(token == "<<")  return Operator::Shl;
            if(token == ">>")  return Operator::Shr;
            if(token == "~")   return Operator::Not;
            return std::nullopt;
        };

        // Parse primary expression (literal, symbol, or parenthesized).
        constexpr auto parse_primary()
            -> std::optional<Expression>
        {
            const auto token = this->peek();
            if(!token)
            {
                return std::nullopt;
            };

            // Handle parenthesized expressions.
            if(token->value == "(")
            {
                this->advance();
                auto expr = this->parse_expression();
                if(!expr)
                {
                    return std::nullopt;
                };

                // Expect closing parenthesis.
                if(!this->peek() || this->peek()->value != ")")
                {
                    return std::nullopt;
                };
                this->advance();

                return expr;
            };

            // Handle unary operators.
            if(token->value == "-" || token->value == "~")
            {
                const auto op = this->token_to_operator(token->value);
                if(!op)
                {
                    return std::nullopt;
                };

                this->advance();
                auto operand = this->parse_primary();
                if(!operand)
                {
                    return std::nullopt;
                };

                return Expression::unary(*op == Operator::Sub ? Operator::Neg : *op, std::move(*operand));
            };

            // Handle literals.
            if(token->type == lexer::TokenType::Literal)
            {
                this->advance();
                return Expression::constant(this->parse_literal(token->value));
            };

            // Handle registers and symbols.
            if(token->type == lexer::TokenType::Register ||
               token->type == lexer::TokenType::Keyword)
            {
                this->advance();
                return Expression::symbol(token->value);
            };

            // Handle memory access [expr].
            if(token->type == lexer::TokenType::Memory)
            {
                this->advance();

                // Remove brackets and parse inner expression.
                auto inner = token->value;
                if(inner.starts_with('[') && inner.ends_with(']'))
                {
                    inner = inner.substr(1, inner.size() - 2);
                };

                // Recursively parse the memory expression.
                Parser inner_parser
                {
                    .current = this->current,
                    .end     = this->end,
                };

                auto base_expr = inner_parser.parse_expression();
                if(!base_expr)
                {
                    return std::nullopt;
                };

                return Expression::memory(std::move(*base_expr));
            };

            return std::nullopt;
        };

        // Parse expression with operator precedence.
        constexpr auto parse_expression()
            -> std::optional<Expression>
        {
            // Start with primary.
            auto left = this->parse_primary();
            if(!left)
            {
                return std::nullopt;
            };

            // Build token stream for Expression::parse.
            Expression::Tokens tokens;

            // Add first operand.
            if(auto constant = left->constant())
            {
                tokens.push_back({ *constant, Operator::Add }); // Dummy operator.
            }
            else if(auto symbol = left->symbol())
            {
                tokens.push_back({ Expression::Symbol{ *symbol }, Operator::Add });
            }
            else
            {
                // Complex expression, return as-is.
                return left;
            };

            // Parse remaining binary operations.
            while(this->peek())
            {
                const auto op_token = this->peek();

                // Check if it's an operator.
                auto op = this->token_to_operator(op_token->value);
                if(!op)
                {
                    break;
                };

                this->advance();

                // Parse right operand.
                auto right = this->parse_primary();
                if(!right)
                {
                    break;
                };

                // Add to token stream.
                if(auto constant = right->constant())
                {
                    tokens.push_back({ *constant, *op });
                }
                else if(auto symbol = right->symbol())
                {
                    tokens.push_back({ Expression::Symbol{ *symbol }, *op });
                }
                else
                {
                    // Build manually for complex expressions.
                    return Expression::binary(*op, std::move(*left), std::move(*right));
                };

                left = std::move(right);
            };

            // Parse token stream if we have operators.
            if(tokens.size() > 1)
            {
                auto result = Expression::parse(tokens);
                if(result.has_value())
                {
                    return std::move(*result);
                };
            };

            return left;
        };

        // Parse operand with traits.
        constexpr auto parse_operand()
            -> std::optional<Operand>
        {
            const auto token = this->peek();
            if(!token)
            {
                return std::nullopt;
            };

            Traits operand_traits;

            // Memory operand.
            if(token->type == lexer::TokenType::Memory)
            {
                operand_traits = Traits{ traits::Source::Memory, traits::Bitness::b64 };

                auto expr = this->parse_expression();
                if(!expr)
                {
                    return std::nullopt;
                };

                return Operand
                {
                    .expression = std::move(*expr),
                    .traits     = operand_traits,
                };
            };

            // Register operand.
            if(token->type == lexer::TokenType::Register)
            {
                // Lookup in architecture.
                auto it = arch.keywords.find(token->value);
                if(it != arch.keywords.cend())
                {
                    auto expr = this->parse_expression();
                    if(!expr)
                    {
                        return std::nullopt;
                    };

                    return Operand
                    {
                        .expression = std::move(*expr),
                        .traits     = it->second,
                    };
                };
            };

            // Literal operand.
            if(token->type == lexer::TokenType::Literal)
            {
                operand_traits = Traits{ traits::Source::Immediate, traits::Bitness::b64 };

                auto expr = this->parse_expression();
                if(!expr)
                {
                    return std::nullopt;
                };

                return Operand
                {
                    .expression = std::move(*expr),
                    .traits     = operand_traits,
                };
            };

            // Keyword/symbol operand.
            if(token->type == lexer::TokenType::Keyword)
            {
                // Check if it's a known keyword with traits.
                auto it = arch.keywords.find(token->value);
                if(it != arch.keywords.cend())
                {
                    auto expr = this->parse_expression();
                    if(!expr)
                    {
                        return std::nullopt;
                    };

                    return Operand
                    {
                        .expression = std::move(*expr),
                        .traits     = it->second,
                    };
                };

                // Unknown keyword, treat as symbol.
                operand_traits = traits::Source::Memory;

                auto expr = this->parse_expression();
                if(!expr)
                {
                    return std::nullopt;
                };

                return Operand
                {
                    .expression = std::move(*expr),
                    .traits     = operand_traits,
                };
            };

            return std::nullopt;
        };

        // Parse instruction.
        constexpr auto parse_instruction()
            -> std::optional<ir::Instruction>
        {
            const auto insn_token = this->expect(lexer::TokenType::Instruction);
            if(!insn_token)
            {
                return std::nullopt;
            };

            std::vector<Operand> operands;

            // Parse operands separated by commas.
            while(true)
            {
                auto operand = this->parse_operand();
                if(!operand)
                {
                    break;
                };

                operands.push_back(std::move(*operand));

                // Check for comma.
                if(!this->peek() || this->peek()->value != ",")
                {
                    break;
                };
                this->advance();
            };

            return ir::Instruction
            {
                .name     = insn_token->value,
                .operands = std::move(operands),
                .line     = insn_token->line,
            };
        };

        // Parse label.
        constexpr auto parse_label()
            -> std::optional<ir::Label>
        {
            const auto label_token = this->expect(lexer::TokenType::Label);
            if(!label_token)
            {
                return std::nullopt;
            };

            // Remove trailing colon.
            auto name = label_token->value;
            if(name.ends_with(':'))
            {
                name = name.substr(0, name.size() - 1);
            };

            return ir::Label
            {
                .name = name,
                .line = label_token->line,
            };
        };

        // Parse data directive using architecture keywords to determine size.
        constexpr auto parse_data()
            -> std::optional<ir::Data>
        {
            const auto keyword_token = this->peek();
            if(!keyword_token || keyword_token->type != lexer::TokenType::Keyword)
            {
                return std::nullopt;
            };

            // Look up the keyword in the architecture to get its traits.
            auto keyword_it = arch.keywords.find(keyword_token->value);
            if(keyword_it == arch.keywords.cend())
            {
                return std::nullopt;
            };

            // Extract bitness from the keyword's traits.
            const auto size = keyword_it->second.template get_as<traits::Bitness>();

            // If the keyword has no bitness trait, it's not a data size keyword.
            if(size == traits::Bitness{})
            {
                return std::nullopt;
            };

            this->advance();

            // Parse label.
            const auto label = this->expect(lexer::TokenType::Label);
            if(!label)
            {
                return std::nullopt;
            };

            auto label_name = label->value;
            if(label_name.ends_with(':'))
            {
                label_name = label_name.substr(0, label_name.size() - 1);
            };

            // Parse values separated by commas.
            std::vector<Expression> values;

            while(true)
            {
                auto expr = this->parse_expression();
                if(!expr)
                {
                    break;
                };

                values.push_back(std::move(*expr));

                if(!this->peek() || this->peek()->value != ",")
                {
                    break;
                };
                this->advance();
            };

            return ir::Data
            {
                .label  = label_name,
                .size   = size,
                .values = std::move(values),
                .line   = keyword_token->line,
            };
        };

        // Parse entire program.
        constexpr auto parse()
            -> ir::Program
        {
            ir::Program program;

            while(this->peek() && this->peek()->type != lexer::TokenType::Eof)
            {
                this->skip_noise();

                if(!this->peek() || this->peek()->type == lexer::TokenType::Eof)
                {
                    break;
                };

                // Try parse data directive.
                if(auto data = this->parse_data())
                {
                    program.add_data(std::move(*data));
                    continue;
                };

                // Try parse label.
                if(auto label = this->parse_label())
                {
                    program.add_label(std::move(*label));
                    continue;
                };

                // Try parse instruction.
                if(auto insn = this->parse_instruction())
                {
                    program.add_instruction(std::move(*insn));
                    continue;
                };

                // Unknown token, skip.
                this->advance();
            };

            return program;
        };

        // Parse from token stream.
        static constexpr auto from(const Tokens& tokens)
            -> ir::Program
        {
            Parser parser
            {
                .current = tokens.begin(),
                .end     = tokens.end(),
            };

            return parser.parse();
        };
    };
};
