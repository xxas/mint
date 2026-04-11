import std;
import xxas;
import mint;

using namespace mint;

// Define the instruction set.
// Each lambda is a handler with access to execution state and operands.
// handler_of<arc>("name") bridges these into HandlerFn<arc> pointers.
constexpr static auto insns = arch::Insns
{
    std::pair{"mov", [](auto& state, auto ops)
    {
        state.template reg<std::uint64_t>(ops[0].value) =
            state.template resolve<std::uint64_t>(ops[1]);
    }},
    std::pair{"add", [](auto& state, auto ops)
    {
        auto a = state.template resolve<std::uint64_t>(ops[1]);
        auto b = state.template resolve<std::uint64_t>(ops[2]);
        state.template reg<std::uint64_t>(ops[0].value) = a + b;
    }},
    std::pair{"sub", [](auto& state, auto ops)
    {
        auto a = state.template resolve<std::uint64_t>(ops[1]);
        auto b = state.template resolve<std::uint64_t>(ops[2]);
        state.template reg<std::uint64_t>(ops[0].value) = a - b;
    }},
    std::pair{"mul", [](auto& state, auto ops)
    {
        auto a = state.template resolve<std::uint64_t>(ops[1]);
        auto b = state.template resolve<std::uint64_t>(ops[2]);
        state.template reg<std::uint64_t>(ops[0].value) = a * b;
    }},
    std::pair{"load", [](auto& state, auto ops)
    {
        auto addr = state.template resolve<std::uint64_t>(ops[1]);
        auto val  = state.program->template read_data<std::uint64_t>(addr);
        state.template reg<std::uint64_t>(ops[0].value) = val.value_or(0);
    }},
    std::pair{"blt", [](auto& state, auto ops)
    {
        auto a      = state.template resolve<std::uint64_t>(ops[0]);
        auto b      = state.template resolve<std::uint64_t>(ops[1]);
        auto target = state.template resolve<std::uint64_t>(ops[2]);
        if(a < b) { state.jump(target); };
    }},
    std::pair{"prntln", [](auto& state, auto ops)
    {
        std::println("{}", state.template resolve<std::uint64_t>(ops[0]));
    }},
    std::pair{"halt", [](auto& state, auto)
    {
        state.halt(0);
    }},
};

// Define language keywords / registers.
constexpr static auto keywords = arch::Keywords
{
    // General-purpose registers.
    std::pair{"gp0", Traits{traits::Bitness::b64, traits::Source::Register}},
    std::pair{"gp1", Traits{traits::Bitness::b64, traits::Source::Register}},
    std::pair{"gp2", Traits{traits::Bitness::b64, traits::Source::Register}},
    std::pair{"gp3", Traits{traits::Bitness::b64, traits::Source::Register}},
    std::pair{"gp4", Traits{traits::Bitness::b64, traits::Source::Register}},
    std::pair{"gp5", Traits{traits::Bitness::b64, traits::Source::Register}},

    // Modifiers.
    std::pair{"dword", Traits{traits::Bitness::b64}},
    std::pair{"word",  Traits{traits::Bitness::b32}},
    std::pair{"ptr",   Traits{traits::Source::Memory}},
};

// Architecture.
constexpr static auto arc = Arch
{
    insns,
    keywords,
};

// Lexer rules.
constexpr static auto rules = lexer::Rules
{
    std::array
    {
        lexer::Pattern{ '[', ']', lexer::TokenType::Memory },

        lexer::Pattern{ lexer::Pattern::Type::Operator, "+", lexer::TokenType::Operator },
        lexer::Pattern{ lexer::Pattern::Type::Operator, "-", lexer::TokenType::Operator },
        lexer::Pattern{ lexer::Pattern::Type::Operator, "*", lexer::TokenType::Operator },
        lexer::Pattern{ lexer::Pattern::Type::Operator, ",", lexer::TokenType::Delimiter },
    }
}.with_comments("#");


// Encoding format:

// Fixed-size encoded instruction. Trivially copyable.
// The `kinds` byte stores 2-bit operand classification per slot,
// eliminating the decoder's need to guess Reg vs Imm from the value.
//
//   bits [1:0] = kind of operand a
//   bits [3:2] = kind of operand b
//   bits [5:4] = kind of operand c
//
//   0b00 = Imm, 0b01 = Reg, 0b10 = Addr
//
struct EncodedInsn
{
    std::uint16_t op;
    std::uint8_t  argc;
    std::uint8_t  kinds = 0;

    std::uint64_t a = 0;
    std::uint64_t b = 0;
    std::uint64_t c = 0;
};


// Uses keyword_ordinal to produce indices that directly index
// the executor's flat register file.
static constexpr auto reg_ordinal(std::string_view name)
    -> std::optional<std::uint64_t>
{
    auto ord = keyword_ordinal<arc>(name);
    if(!ord) return std::nullopt;
    return static_cast<std::uint64_t>(*ord);
};

// Classify an operand's source trait into a 2-bit kind tag.
static constexpr auto operand_kind(const Operand& operand)
    -> std::uint8_t
{
    auto src = operand.traits.get_as<traits::Source>();

    if(src == traits::Source::Register) return 0b01; // Reg
    if(src == traits::Source::Memory)   return 0b10; // Addr
    return 0b00; // Imm
};


// Encodes IR instructions into fixed-size binary format.
struct SimpleEncoder
{
    using SymbolResolver = lowering::SymbolResolver;

    auto resolve_operand(const Operand& operand, const SymbolResolver& resolver) const
        -> std::optional<std::uint64_t>
    {
        return operand.expression.evaluate<std::uint64_t>(
            [&](std::string_view name) -> std::optional<std::uint64_t>
            {
                // Try register ordinal first (CMap-indexed).
                if(auto ord = reg_ordinal(name))
                {
                    return ord;
                };

                // Fall through to symbol resolution.
                return resolver(name);
            }
        );
    };

    auto encode(const ir::Instruction& insn, const SymbolResolver& resolver) const
        -> std::optional<std::vector<std::byte>>
    {
        auto opcode = insn_opcode<arc>(insn.name);
        if(!opcode) return std::nullopt;

        EncodedInsn encoded
        {
            .op   = *opcode,
            .argc = static_cast<std::uint8_t>(insn.operands.size()),
        };

        // Encode operand values and kind tags.
        auto encode_operand = [&](std::uint8_t index, std::uint64_t& dest) -> bool
        {
            if(index >= insn.operands.size()) return true;

            auto val = this->resolve_operand(insn.operands[index], resolver);
            if(!val) return false;

            dest = *val;

            // Pack the 2-bit kind tag into the kinds byte.
            encoded.kinds |= (operand_kind(insn.operands[index]) << (index * 2));
            return true;
        };

        if(!encode_operand(0, encoded.a)) return std::nullopt;
        if(!encode_operand(1, encoded.b)) return std::nullopt;
        if(!encode_operand(2, encoded.c)) return std::nullopt;

        auto bytes = xxas::encode(encoded);
        return std::vector<std::byte>{ bytes.begin(), bytes.end() };
    };

    auto estimate_size(const ir::Instruction&) const
        -> std::size_t
    {
        return sizeof(EncodedInsn);
    };
};


// Decodes binary format back into flat decoded instructions.
// Inverse of SimpleEncoder.
struct SimpleDecoder
{
    // Map 2-bit kind tag back to lift::Kind.
    static constexpr auto decode_kind(std::uint8_t kinds, std::uint8_t index)
        -> lift::Kind
    {
        const auto k = (kinds >> (index * 2)) & 0x3;

        if(k == 0b01) return lift::Kind::Reg;
        if(k == 0b10) return lift::Kind::Addr;
        return lift::Kind::Imm;
    };

    auto decode(std::span<const std::byte> bytes) const
        -> std::optional<lift::Decoded>
    {
        if(bytes.size() < sizeof(EncodedInsn))
        {
            return std::nullopt;
        };

        const auto insn = xxas::decode<EncodedInsn>(bytes.subspan(0, sizeof(EncodedInsn)));

        lift::Decoded result
        {
            .opcode        = insn.op,
            .operand_count = insn.argc,
            .byte_size     = static_cast<std::uint32_t>(sizeof(EncodedInsn)),
        };

        // Use the kinds byte to classify each operand.
        auto classify = [&](std::uint8_t index, std::uint64_t value)
        {
            result.operands[index] = lift::Operand
            {
                decode_kind(insn.kinds, index),
                value
            };
        };

        if(insn.argc >= 1) classify(0, insn.a);
        if(insn.argc >= 2) classify(1, insn.b);
        if(insn.argc >= 3) classify(2, insn.c);

        return result;
    };
};


// Auto-generated dispatch table.
constexpr static auto dispatch = exec::insn_dispatch<arc>();

// Convenience aliases.
using Obj     = Object<arc, rules>;
using Builder = InstanceBuilder<arc, dispatch, SimpleEncoder, SimpleDecoder>;


int main()
{
    constexpr std::string_view source =
    R"(
    dword values: 10, 20, 30, 40

    main:

        # --- Sum array elements ---
        mov     gp0, 0              # sum = 0
        mov     gp1, 0              # i = 0
        mov     gp2, values         # ptr = &values

    sum_loop:
        load    gp3, gp2            # gp3 = data[ptr]
        add     gp0, gp0, gp3      # sum += gp3
        add     gp2, gp2, 8        # ptr += sizeof(dword)
        add     gp1, gp1, 1        # i++
        blt     gp1, 4, sum_loop   # if i < 4, loop

        prntln  gp0                 # -> 100

        # --- Compute 5! (factorial) ---
        mov     gp0, 1              # result = 1
        mov     gp1, 2              # i = 2

    fact_loop:
        mul     gp0, gp0, gp1      # result *= i
        add     gp1, gp1, 1        # i++
        blt     gp1, 6, fact_loop  # if i < 6, loop (2*3*4*5)

        prntln  gp0                 # -> 120

        # Dot product of values [1, 2, 3, 4]
        mov     gp0, 0              # dot = 0
        mov     gp1, 0              # i = 0
        mov     gp2, values         # ptr = &values
        mov     gp3, 1              # weight = 1

    dot_loop:
        load    gp4, gp2            # gp4 = values[i]
        mul     gp5, gp4, gp3      # gp5 = values[i] * weight
        add     gp0, gp0, gp5      # dot += gp5
        add     gp2, gp2, 8        # ptr += sizeof(dword)
        add     gp3, gp3, 1        # weight++
        add     gp1, gp1, 1        # i++
        blt     gp1, 4, dot_loop   # if i < 4, loop

        prntln  gp0                 # -> 10*1 + 20*2 + 30*3 + 40*4 = 300

        halt
    )";

    // Build: compile object, link, produce executable.
    auto instance = Builder{}
        .add(Obj::from(source, "main"))
        .build();

    if(!instance)
    {
        std::println("build failed: {}", instance.error().message);
        return 1;
    };

    // Inspect lifted code.
    for(const auto& insn: instance->code())
    {
        const auto ops = instance->operands_of(insn);

        std::print("[op={}] ", insn.opcode);
        for(const auto& op: ops)
        {
            auto kind_str = op.kind == lift::Kind::Reg  ? "reg" :
                            op.kind == lift::Kind::Imm  ? "imm" : "addr";
            std::print("{}:{} ", kind_str, op.value);
        };
        std::println("");
    };

    // Execute.
    auto result = instance->run();

    if(!result)
    {
        std::println("runtime error: {}", result.error().message);
        return 1;
    };

    std::println("exit code: {}", *result);
    return 0;
};
