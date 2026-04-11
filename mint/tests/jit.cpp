import std;
import xxas;
import mint;

namespace mint_tests
{
    using namespace mint;

    constexpr static auto keywords = arch::Keywords
    {
        std::pair{"gp0", Traits{traits::Bitness::b64, traits::Source::Register}},
    };

    constexpr static auto insns = arch::Insns
    {
        std::pair{"halt", [](auto& state, auto)
        {
            state.halt(0);
        }},
    };

    constexpr inline Arch arch
    {
        insns, keywords
    };

    constexpr static auto rules = lexer::Rules
    {
        std::array
        {
            lexer::Pattern{ lexer::Pattern::Type::Operator, ",", lexer::TokenType::Delimiter },
        }
    };

    constexpr static auto dispatch = exec::insn_dispatch<arch>();

    // Minimal encoder/decoder for the halt instruction.
    struct TestInsn
    {
        std::uint16_t op;
        std::uint8_t  argc = 0;
        std::uint8_t  pad  = 0;
    };

    struct TestEncoder
    {
        auto encode(const ir::Instruction& insn, const lowering::SymbolResolver&) const
            -> std::optional<std::vector<std::byte>>
        {
            auto opcode = insn_opcode<arch>(insn.name);
            if(!opcode) return std::nullopt;

            auto bytes = xxas::encode(TestInsn{ .op = *opcode });
            return std::vector<std::byte>{ bytes.begin(), bytes.end() };
        };

        auto estimate_size(const ir::Instruction&) const
            -> std::size_t
        {
            return sizeof(TestInsn);
        };
    };

    struct TestDecoder
    {
        auto decode(std::span<const std::byte> bytes) const
            -> std::optional<lift::Decoded>
        {
            if(bytes.size() < sizeof(TestInsn)) return std::nullopt;

            const auto insn = xxas::decode<TestInsn>(bytes.subspan(0, sizeof(TestInsn)));

            return lift::Decoded
            {
                .opcode        = insn.op,
                .operand_count = insn.argc,
                .byte_size     = static_cast<std::uint32_t>(sizeof(TestInsn)),
            };
        };
    };

    using Obj     = Object<arch, rules>;
    using Builder = InstanceBuilder<arch, dispatch, TestEncoder, TestDecoder>;

    void instance_build_and_run()
    {
        auto vm = Builder{}
            .add(Obj::from("main:\n    halt", "main"))
            .build();

        xxas::assert(vm.has_value());

        auto result = vm->run();
        xxas::assert(result.has_value());
        xxas::assert(*result == 0);
    };


    constexpr xxas::Tests jit
    {
        instance_build_and_run,
    };
};

int main()
{
    return mint_tests::jit();
}
