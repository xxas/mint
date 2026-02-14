import std;
import xxas;
import mint;

namespace mint_tests
{
    using namespace mint;

    // Architecture definition for lexer tests (same as arch.cpp / jit.cpp).
    constexpr static auto keywords = arch::Keywords
    {
        std::pair{"gp0",   Traits{traits::Bitness::b64, traits::Source::Register}},
        std::pair{"gp1",   Traits{traits::Bitness::b64, traits::Source::Register}},
        std::pair{"gp2",   Traits{traits::Bitness::b64, traits::Source::Register}},

        std::pair{"dword", Traits{traits::Bitness::b64}},
        std::pair{"word",  Traits{traits::Bitness::b32}},
        std::pair{"ptr",   Traits{traits::Source::Memory}},
    };

    constexpr static auto insns = arch::Insns
    {
        std::pair{"mov",    [](auto& dest, const auto& src) -> void { dest = src; }},
        std::pair{"add",    [](auto& dest, const auto& src) -> void { dest += src; }},
        std::pair{"sub",    [](auto& dest, const auto& src) -> void { dest -= src; }},
        std::pair{"prntln", [](const auto& src) -> void { std::println("{}", src); }},
        std::pair{"halt",   []() -> void {}},
    };

    constexpr static auto test_arch = Arch
    {
        insns, keywords
    };

    // Lexer rules configuration.
    constexpr static auto rules = lexer::Rules
    {
        std::array
        {   // Memory operand: [expr].
            lexer::Pattern('[', ']', lexer::TokenType::Memory),

            // Operators.
            lexer::Pattern(lexer::Pattern::Type::Operator, "+",  lexer::TokenType::Operator),
            lexer::Pattern(lexer::Pattern::Type::Operator, "-",  lexer::TokenType::Operator),
            lexer::Pattern(lexer::Pattern::Type::Operator, "*",  lexer::TokenType::Operator),
            lexer::Pattern(lexer::Pattern::Type::Operator, "<<", lexer::TokenType::Operator),
            lexer::Pattern(lexer::Pattern::Type::Operator, ">>", lexer::TokenType::Operator),
        }
    };

    // NOTE: The lexer template references `Arch.instructions` but the Arch struct
    // has `insns`, and references `traits.source` but Traits uses `get_as<>()`.
    // These tests assume those two naming fixes have been applied in the lexer:
    //   1. Arch.instructions -> Arch.insns
    //   2. traits.source     -> traits.get_as<traits::Source>()

    using TestLexer = lexer::Lexer<test_arch, rules>;

    // Helper: count tokens of a given type.
    auto count_type(const TestLexer::Tokens& tokens, lexer::TokenType type)
        -> std::size_t
    {
        return std::ranges::count_if(tokens, [type](const auto& token)
        {
            return token.type == type;
        });
    };

    // Helper: find first token of a given type.
    auto find_first(const TestLexer::Tokens& tokens, lexer::TokenType type)
        -> std::optional<lexer::Token>
    {
        auto it = std::ranges::find_if(tokens, [type](const auto& token)
        {
            return token.type == type;
        });

        if(it != tokens.end())
        {
            return *it;
        };

        return std::nullopt;
    };

    // Empty source should produce only an Eof token.
    void tokenize_empty()
    {
        auto tokens = TestLexer::from("");

        xxas::assert_eq(tokens.size(), 1u);
        xxas::assert_eq(tokens.back().type, lexer::TokenType::Eof);
    };

    // Whitespace-only source should produce only Eof.
    void tokenize_whitespace()
    {
        auto tokens = TestLexer::from("   \t\t   ");

        xxas::assert_eq(tokens.size(), 1u);
        xxas::assert_eq(tokens.back().type, lexer::TokenType::Eof);
    };

    // Single instruction mnemonic.
    void tokenize_instruction()
    {
        auto tokens = TestLexer::from("mov");

        auto insn = find_first(tokens, lexer::TokenType::Instruction);
        xxas::assert(insn.has_value(), "instruction token found");
        xxas::assert_eq(insn->value, std::string_view{"mov"});
    };

    // Register keyword should tokenize as Register type.
    void tokenize_register()
    {
        auto tokens = TestLexer::from("gp0");

        auto reg = find_first(tokens, lexer::TokenType::Register);
        xxas::assert(reg.has_value(), "register token found");
        xxas::assert_eq(reg->value, std::string_view{"gp0"});
    };

    // Non-register keyword should tokenize as Keyword type.
    void tokenize_keyword()
    {
        auto tokens = TestLexer::from("dword");

        auto kw = find_first(tokens, lexer::TokenType::Keyword);
        xxas::assert(kw.has_value(), "keyword token found");
        xxas::assert_eq(kw->value, std::string_view{"dword"});
    };

    // Decimal literal.
    void tokenize_decimal_literal()
    {
        auto tokens = TestLexer::from("42");

        auto lit = find_first(tokens, lexer::TokenType::Literal);
        xxas::assert(lit.has_value(), "literal token found");
        xxas::assert_eq(lit->value, std::string_view{"42"});
    };

    // Hex literal.
    void tokenize_hex_literal()
    {
        auto tokens = TestLexer::from("0xFF");

        auto lit = find_first(tokens, lexer::TokenType::Literal);
        xxas::assert(lit.has_value(), "hex literal found");
        xxas::assert_eq(lit->value, std::string_view{"0xFF"});
    };

    // Binary literal.
    void tokenize_binary_literal()
    {
        auto tokens = TestLexer::from("0b1010");

        auto lit = find_first(tokens, lexer::TokenType::Literal);
        xxas::assert(lit.has_value(), "binary literal found");
        xxas::assert_eq(lit->value, std::string_view{"0b1010"});
    };

    // Octal literal.
    void tokenize_octal_literal()
    {
        auto tokens = TestLexer::from("0o777");

        auto lit = find_first(tokens, lexer::TokenType::Literal);
        xxas::assert(lit.has_value(), "octal literal found");
        xxas::assert_eq(lit->value, std::string_view{"0o777"});
    };

    // Underscore-separated literal.
    void tokenize_underscore_literal()
    {
        auto tokens = TestLexer::from("1_000_000");

        auto lit = find_first(tokens, lexer::TokenType::Literal);
        xxas::assert(lit.has_value(), "underscore literal found");
        xxas::assert_eq(lit->value, std::string_view{"1_000_000"});
    };

    // Label definition (identifier followed by colon).
    void tokenize_label()
    {
        auto tokens = TestLexer::from("main:");

        auto label = find_first(tokens, lexer::TokenType::Label);
        xxas::assert(label.has_value(), "label token found");
        xxas::assert_eq(label->value, std::string_view{"main:"});
    };

    // Memory operand with brackets.
    void tokenize_memory()
    {
        auto tokens = TestLexer::from("[gp0]");

        auto mem = find_first(tokens, lexer::TokenType::Memory);
        xxas::assert(mem.has_value(), "memory token found");
        xxas::assert_eq(mem->value, std::string_view{"[gp0]"});
    };

    // Nested brackets in memory operand.
    void tokenize_nested_memory()
    {
        auto tokens = TestLexer::from("[gp0 + [gp1]]");

        auto mem = find_first(tokens, lexer::TokenType::Memory);
        xxas::assert(mem.has_value(), "nested memory token found");

        // Should capture the full delimited range including nested brackets.
        xxas::assert_eq(mem->value, std::string_view{"[gp0 + [gp1]]"});
    };

    // Operator tokens.
    void tokenize_operators()
    {
        auto tokens = TestLexer::from("+ - * << >>");

        auto op_count = count_type(tokens, lexer::TokenType::Operator);
        xxas::assert_eq(op_count, 5u);
    };

    // Comment should be captured as a Comment token.
    void tokenize_comment()
    {
        auto tokens = TestLexer::from("# this is a comment");

        auto comment = find_first(tokens, lexer::TokenType::Comment);
        xxas::assert(comment.has_value(), "comment token found");
    };

    // Newline should produce a Newline token.
    void tokenize_newline()
    {
        auto tokens = TestLexer::from("mov\nhalt");

        auto nl_count = count_type(tokens, lexer::TokenType::Newline);
        xxas::assert_eq(nl_count, 1u);
    };

    // Delimiter characters (comma, etc.).
    void tokenize_delimiter()
    {
        auto tokens = TestLexer::from(",");

        auto delim = find_first(tokens, lexer::TokenType::Delimiter);
        xxas::assert(delim.has_value(), "delimiter token found");
        xxas::assert_eq(delim->value, std::string_view{","});
    };

    // A full instruction line: `mov gp0, 0xFF`.
    void tokenize_full_instruction()
    {
        auto tokens = TestLexer::from("mov gp0, 0xFF");

        // Should have: Instruction, Register, Delimiter, Literal, Eof.
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Instruction), 1u);
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Register),    1u);
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Delimiter),   1u);
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Literal),     1u);
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Eof),         1u);
    };

    // Multi-line program tokenization.
    void tokenize_multiline_program()
    {
        constexpr auto source =
            "main:\n"
            "    mov gp0, 42\n"
            "    add gp0, gp1\n"
            "    halt\n"
            "    # done\n";

        auto tokens = TestLexer::from(source);

        // Check expected token counts.
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Label),       1u);
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Instruction), 3u);
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Comment),     1u);
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Eof),         1u);

        // At least 4 newlines.
        xxas::assert(count_type(tokens, lexer::TokenType::Newline) >= 4,
            "at least 4 newlines");
    };

    // Line and column tracking.
    void tokenize_line_column()
    {
        auto tokens = TestLexer::from("mov\nhalt");

        // "mov" should be on line 1.
        auto mov_token = find_first(tokens, lexer::TokenType::Instruction);
        xxas::assert(mov_token.has_value(), "mov found");
        xxas::assert_eq(mov_token->line, 1u);
        xxas::assert_eq(mov_token->column, 1u);

        // "halt" should be on line 2.
        // Find second instruction.
        auto it = std::ranges::find_if(tokens, [](const auto& t)
        {
            return t.type == lexer::TokenType::Instruction && t.value == "halt";
        });
        xxas::assert(it != tokens.end(), "halt found");
        xxas::assert_eq(it->line, 2u);
    };

    // Partial match: "moving" should not match "mov".
    void tokenize_partial_no_match()
    {
        auto tokens = TestLexer::from("moving");

        // "moving" is not "mov" followed by word boundary, so it
        // should tokenize as a keyword/identifier, not an instruction.
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Instruction), 0u);
    };

    // Memory operand with expression.
    void tokenize_memory_expression()
    {
        auto tokens = TestLexer::from("mov gp0, [gp1 + 8]");

        xxas::assert_eq(count_type(tokens, lexer::TokenType::Instruction), 1u);
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Register),    1u);
        xxas::assert_eq(count_type(tokens, lexer::TokenType::Memory),      1u);
    };

    // Token stream always ends with Eof.
    void tokenize_always_ends_eof()
    {
        auto tokens_a = TestLexer::from("");
        auto tokens_b = TestLexer::from("mov gp0, gp1");
        auto tokens_c = TestLexer::from("# just a comment");

        xxas::assert_eq(tokens_a.back().type, lexer::TokenType::Eof);
        xxas::assert_eq(tokens_b.back().type, lexer::TokenType::Eof);
        xxas::assert_eq(tokens_c.back().type, lexer::TokenType::Eof);
    };

    constexpr xxas::Tests lexer
    {
        tokenize_empty,
        tokenize_whitespace,
        tokenize_instruction,
        tokenize_register,
        tokenize_keyword,
        tokenize_decimal_literal,
        tokenize_hex_literal,
        tokenize_binary_literal,
        tokenize_octal_literal,
        tokenize_underscore_literal,
        tokenize_label,
        tokenize_memory,
        tokenize_nested_memory,
        tokenize_operators,
        tokenize_comment,
        tokenize_newline,
        tokenize_delimiter,
        tokenize_full_instruction,
        tokenize_multiline_program,
        tokenize_line_column,
        tokenize_partial_no_match,
        tokenize_memory_expression,
        tokenize_always_ends_eof,
    };
};

int main()
{
    return mint_tests::lexer();
};
