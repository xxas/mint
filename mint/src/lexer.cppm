export module mint: lexer;

import std;
import xxas;
import :traits;

namespace mint
{
    namespace lexer
    {
        // Token type identifiers.
        export enum class TokenType: std::uint32_t
        {
            Unknown = 0,
            Instruction,
            Keyword,
            Register,
            Literal,
            Label,
            Memory,
            Operator,
            Delimiter,
            Comment,
            Newline,
            Eof,
        };

        export struct Token
        {
            TokenType           type;
            std::string_view    value;
            std::size_t         line;
            std::size_t         column;

            constexpr Token(TokenType type, std::string_view value, std::size_t line, std::size_t column)
                : type{ type }, value{ value }, line{ line }, column{ column } {};
        };

        // Pattern matching rules.
        export struct Pattern
        {
            enum class Type: std::uint8_t
            {
                Literal,        // Exact match.
                Regex,          // Regex pattern.
                Delimited,      // Bracketed expressions like [expr].
                Operator,       // Multi-char operators.
                Whitespace,     // Skip whitespace.
                Comment,        // Comment to EOL.
            };

            Type                type;
            std::string_view    pattern;
            TokenType           token_type;
            char                delimiter_start{ '\0' };
            char                delimiter_end{ '\0' };

            constexpr Pattern(Type type, std::string_view pattern, TokenType token_type)
                : type{ type }, pattern{ pattern }, token_type{ token_type } {};

            constexpr Pattern(char start, char end, TokenType token_type)
                : type{ Type::Delimited }, token_type{ token_type }, delimiter_start{ start }, delimiter_end{ end } {};
        };

        // Lexer rules configuration.
        export template<std::size_t N> struct Rules
        {
            std::array<Pattern, N>  patterns;
            std::string_view        comment_marker{ "#" };
            bool                    case_sensitive{ true };

            constexpr Rules(const std::array<Pattern, N>& patterns)
                : patterns{ patterns } {};

            constexpr auto with_comments(std::string_view marker)
                -> Rules&
            {
                this->comment_marker = marker;
                return *this;
            };

            constexpr auto case_insensitive()
                -> Rules&
            {
                this->case_sensitive = false;
                return *this;
            };
        };

        // Lexer state.
        struct State
        {
            std::string_view    source;
            std::size_t         position{ 0 };
            std::size_t         line{ 1 };
            std::size_t         column{ 1 };

            constexpr State(std::string_view source)
                : source{ source } {};

            // Peek current character.
            constexpr auto peek() const
                -> std::optional<char>
            {
                if(this->position >= this->source.size())
                {
                    return std::nullopt;
                };

                return this->source[this->position];
            };

            // Peek ahead n characters.
            constexpr auto peek(std::size_t offset) const
                -> std::optional<char>
            {
                const auto pos = this->position + offset;
                if(pos >= this->source.size())
                {
                    return std::nullopt;
                };

                return this->source[pos];
            };

            // Advance by n characters.
            constexpr auto advance(std::size_t count = 1)
                -> void
            {
                for(std::size_t i = 0; i < count && this->position < this->source.size(); ++i)
                {
                    if(this->source[this->position] == '\n')
                    {
                        this->line++;
                        this->column = 1;
                    }
                    else
                    {
                        this->column++;
                    };

                    this->position++;
                };
            };

            // Check if at end.
            constexpr auto is_eof() const
                -> bool
            {
                return this->position >= this->source.size();
            };

            // Skip whitespace.
            constexpr auto skip_whitespace()
                -> void
            {
                while(!this->is_eof())
                {
                    const auto ch = *this->peek();
                    if(ch != ' ' && ch != '\t' && ch != '\r')
                    {
                        break;
                    };

                    this->advance();
                };
            };

            // Extract substring from current position.
            constexpr auto extract(std::size_t length) const
                -> std::string_view
            {
                return this->source.substr(this->position, length);
            };

            // Match literal string at current position.
            constexpr auto match_literal(std::string_view literal, bool case_sensitive = true) const
                -> bool
            {
                if(this->position + literal.size() > this->source.size())
                {
                    return false;
                };

                const auto view = this->extract(literal.size());

                if(case_sensitive)
                {
                    return view == literal;
                };

                return std::ranges::equal(view, literal, [](char a, char b)
                {
                    return std::tolower(a) == std::tolower(b);
                });
            };
        };

        // Main lexer.
        export template<const auto& Arch, const auto& Rules> struct Lexer
        {
            using Tokens = std::vector<Token>;

            State state;

            constexpr Lexer(std::string_view source)
                : state{ source } {};

            // Try to match instruction from architecture.
            constexpr auto try_instruction()
                -> std::optional<Token>
            {
                for(const auto& [name, _] : Arch.insns)
                {
                    if(this->state.match_literal(name, Rules.case_sensitive))
                    {
                        // Check if followed by delimiter.
                        const auto next = this->state.peek(name.size());
                        if(next && std::isalnum(*next))
                        {
                            continue;
                        };

                        const auto token = Token
                        {
                            TokenType::Instruction,
                            name,
                            this->state.line,
                            this->state.column
                        };

                        this->state.advance(name.size());
                        return token;
                    };
                };

                return std::nullopt;
            };

            // Try to match keyword from architecture.
            constexpr auto try_keyword()
                -> std::optional<Token>
            {
                for(const auto& [name, traits] : Arch.keywords)
                {
                    if(this->state.match_literal(name, Rules.case_sensitive))
                    {
                        const auto next = this->state.peek(name.size());
                        if(next && std::isalnum(*next))
                        {
                            continue;
                        };

                        const auto type = (traits.template get_as<traits::Source>() == traits::Source::Register)
                            ? TokenType::Register
                            : TokenType::Keyword;

                        const auto token = Token
                        {
                            type,
                            name,
                            this->state.line,
                            this->state.column
                        };

                        this->state.advance(name.size());
                        return token;
                    };
                };

                return std::nullopt;
            };

            // Try to match delimited pattern (e.g., [expr], (expr)).
            constexpr auto try_delimited(const Pattern& pattern)
                -> std::optional<Token>
            {
                const auto ch = this->state.peek();
                if(!ch || *ch != pattern.delimiter_start)
                {
                    return std::nullopt;
                };

                const auto start_line = this->state.line;
                const auto start_col = this->state.column;
                const auto start_pos = this->state.position;

                this->state.advance();

                // Find matching delimiter.
                std::size_t depth = 1;
                while(!this->state.is_eof())
                {
                    const auto curr = *this->state.peek();

                    if(curr == pattern.delimiter_start)
                    {
                        depth++;
                    }
                    else if(curr == pattern.delimiter_end)
                    {
                        depth--;
                        if(depth == 0)
                        {
                            this->state.advance();
                            const auto length = this->state.position - start_pos;
                            return Token
                            {
                                pattern.token_type,
                                this->state.source.substr(start_pos, length),
                                start_line,
                                start_col
                            };
                        };
                    };

                    this->state.advance();
                };

                return std::nullopt;
            };

            // Try to match operator pattern.
            constexpr auto try_operator(const Pattern& pattern)
                -> std::optional<Token>
            {
                if(this->state.match_literal(pattern.pattern, true))
                {
                    const auto token = Token
                    {
                        pattern.token_type,
                        pattern.pattern,
                        this->state.line,
                        this->state.column
                    };

                    this->state.advance(pattern.pattern.size());
                    return token;
                };

                return std::nullopt;
            };

            // Try to match label (identifier followed by ':').
            constexpr auto try_label()
                -> std::optional<Token>
            {
                const auto start_pos = this->state.position;
                const auto start_line = this->state.line;
                const auto start_col = this->state.column;

                // Match identifier.
                if(!this->state.peek() || !std::isalpha(*this->state.peek()))
                {
                    return std::nullopt;
                };

                while(!this->state.is_eof())
                {
                    const auto ch = *this->state.peek();
                    if(!std::isalnum(ch) && ch != '_')
                    {
                        break;
                    };
                    this->state.advance();
                };

                // Check for colon.
                if(this->state.peek() && *this->state.peek() == ':')
                {
                    this->state.advance();
                    const auto length = this->state.position - start_pos;
                    return Token
                    {
                        TokenType::Label,
                        this->state.source.substr(start_pos, length),
                        start_line,
                        start_col
                    };
                };

                // Not a label, restore position.
                this->state.position = start_pos;
                this->state.line = start_line;
                this->state.column = start_col;

                return std::nullopt;
            };

            // Try to match numeric literal.
            constexpr auto try_number()
                -> std::optional<Token>
            {
                const auto start_pos  = this->state.position;
                const auto start_line = this->state.line;
                const auto start_col  = this->state.column;

                auto ch = this->state.peek();
                if(!ch)
                {
                    return std::nullopt;
                };

                // Handle hex (0x), binary (0b), octal (0o).
                if(*ch == '0')
                {
                    const auto next = this->state.peek(1);
                    if(next && (*next == 'x' || *next == 'b' || *next == 'o'))
                    {
                        this->state.advance(2);

                        while(!this->state.is_eof())
                        {
                            const auto curr = *this->state.peek();
                            if(!std::isxdigit(curr) && curr != '_')
                            {
                                break;
                            };
                            this->state.advance();
                        };

                        const auto length = this->state.position - start_pos;
                        return Token
                        {
                            TokenType::Literal,
                            this->state.source.substr(start_pos, length),
                            start_line,
                            start_col
                        };
                    };
                };

                // Regular decimal number.
                if(!std::isdigit(*ch))
                {
                    return std::nullopt;
                };

                while(!this->state.is_eof())
                {
                    const auto curr = *this->state.peek();
                    if(!std::isdigit(curr) && curr != '_')
                    {
                        break;
                    };
                    this->state.advance();
                };

                const auto length = this->state.position - start_pos;
                return Token
                {
                    TokenType::Literal,
                    this->state.source.substr(start_pos, length),
                    start_line,
                    start_col
                };
            };

            // Try to match identifier.
            constexpr auto try_identifier()
                -> std::optional<Token>
            {
                const auto start_pos = this->state.position;
                const auto start_line = this->state.line;
                const auto start_col = this->state.column;

                const auto ch = this->state.peek();
                if(!ch || (!std::isalpha(*ch) && *ch != '_' && *ch != '.'))
                {
                    return std::nullopt;
                };

                this->state.advance();

                while(!this->state.is_eof())
                {
                    const auto curr = *this->state.peek();
                    if(!std::isalnum(curr) && curr != '_')
                    {
                        break;
                    };
                    this->state.advance();
                };

                const auto length = this->state.position - start_pos;
                return Token
                {
                    TokenType::Keyword,
                    this->state.source.substr(start_pos, length),
                    start_line,
                    start_col
                };
            };

            // Tokenize entire source.
            constexpr auto tokenize()
                -> Tokens
            {
                Tokens tokens;

                while(!this->state.is_eof())
                {
                    this->state.skip_whitespace();

                    if(this->state.is_eof())
                    {
                        break;
                    };

                    // Handle newline.
                    if(*this->state.peek() == '\n')
                    {
                        tokens.emplace_back(TokenType::Newline, "\n", this->state.line, this->state.column);
                        this->state.advance();
                        continue;
                    };

                    // Handle comments.
                    if(this->state.match_literal(Rules.comment_marker, true))
                    {
                        const auto start_pos = this->state.position;
                        const auto start_line = this->state.line;
                        const auto start_col = this->state.column;

                        while(!this->state.is_eof() && *this->state.peek() != '\n')
                        {
                            this->state.advance();
                        };

                        const auto length = this->state.position - start_pos;
                        tokens.emplace_back(TokenType::Comment, this->state.source.substr(start_pos, length), start_line, start_col);
                        continue;
                    };

                    // Try patterns from rules.
                    bool matched = false;
                    for(const auto& pattern: Rules.patterns)
                    {
                        std::optional<Token> token;

                        switch(pattern.type)
                        {
                            case Pattern::Type::Delimited:
                                token = this->try_delimited(pattern);
                                break;

                            case Pattern::Type::Operator:
                                token = this->try_operator(pattern);
                                break;

                            default:
                                break;
                        };

                        if(token)
                        {
                            tokens.push_back(*token);
                            matched = true;
                            break;
                        };
                    };

                    if(matched)
                    {
                        continue;
                    };

                    // Try architecture-specific tokens.
                    if(auto token = this->try_label())
                    {
                        tokens.push_back(*token);
                        continue;
                    };

                    if(auto token = this->try_instruction())
                    {
                        tokens.push_back(*token);
                        continue;
                    };

                    if(auto token = this->try_keyword())
                    {
                        tokens.push_back(*token);
                        continue;
                    };

                    if(auto token = this->try_number())
                    {
                        tokens.push_back(*token);
                        continue;
                    };

                    if(auto token = this->try_identifier())
                    {
                        tokens.push_back(*token);
                        continue;
                    };

                    // Single character tokens.
                    const auto ch = *this->state.peek();
                    tokens.emplace_back(TokenType::Delimiter, this->state.extract(1), this->state.line, this->state.column);
                    this->state.advance();
                };

                tokens.emplace_back(TokenType::Eof, "", this->state.line, this->state.column);
                return tokens;
            };

            // Static interface.
            static constexpr auto from(std::string_view source)
                -> Tokens
            {
                Lexer lexer
                {
                    source
                };

                return lexer.tokenize();
            };
        };
    };
};
