import std;
import xxas;
import mint;

namespace mint_tests
{
    using namespace mint;

    // Virtual address allocation should be aligned and monotonically increasing.
    void vaddr_allocation()
    {
        Assembler assembler{};

        auto first  = assembler.allocate_vaddr(64, 16);
        auto second = assembler.allocate_vaddr(128, 16);
        auto third  = assembler.allocate_vaddr(32, 16);

        // Each allocation should be after the previous.
        xxas::assert(second > first,  "second > first");
        xxas::assert(third  > second, "third > second");

        // Verify alignment.
        xxas::assert_eq(first  % 16, 0u);
        xxas::assert_eq(second % 16, 0u);
        xxas::assert_eq(third  % 16, 0u);
    };

    // Adding code should produce a valid code section and return a vaddr.
    void add_code_bytes()
    {
        Assembler assembler{};

        std::vector<std::byte> instructions
        {
            std::byte{0x48}, std::byte{0x89}, std::byte{0xE5},
            std::byte{0xC3},
        };

        auto vaddr = assembler.add_code(
            std::span<const std::byte>{instructions.data(), instructions.size()},
            "entry"
        );

        xxas::assert(vaddr > 0, "code vaddr > 0");

        // Symbol should be resolvable.
        auto sym_addr = assembler.get_symbol_address("entry");
        xxas::assert(sym_addr.has_value(), "entry symbol found");
        xxas::assert_eq(*sym_addr, vaddr);
    };

    // Adding code from a vector of typed instructions.
    void add_code_vector()
    {
        Assembler assembler{};

        std::vector<std::uint32_t> insns{0x90909090, 0xCCCCCCCC, 0xC3C3C3C3};

        auto vaddr = assembler.add_code(insns, "block");

        xxas::assert(vaddr > 0, "code vector vaddr > 0");

        auto sym_addr = assembler.get_symbol_address("block");
        xxas::assert(sym_addr.has_value(), "block symbol found");
        xxas::assert_eq(*sym_addr, vaddr);
    };

    // String constants should be stored with null termination.
    void add_string_constant()
    {
        Assembler assembler{};

        auto vaddr = assembler.add_string("hello", "greeting");

        xxas::assert(vaddr > 0, "string vaddr > 0");

        auto sym_addr = assembler.get_symbol_address("greeting");
        xxas::assert(sym_addr.has_value(), "greeting symbol found");
    };

    // Duplicate rodata should be deduplicated by the constant pool.
    void rodata_deduplication()
    {
        Assembler assembler{};

        auto first  = assembler.add_constant(std::uint64_t{0xDEADBEEF}, "const_a");
        auto second = assembler.add_constant(std::uint64_t{0xDEADBEEF});

        // Same value should yield same vaddr when deduplicated.
        xxas::assert_eq(first, second);

        // Different value should yield different vaddr.
        auto third = assembler.add_constant(std::uint64_t{0xCAFEBABE}, "const_b");
        xxas::assert_ne(first, third);
    };

    // Initialized data section.
    void add_initialized_data()
    {
        Assembler assembler{};

        std::uint64_t value = 0x1234567890ABCDEF;
        auto vaddr = assembler.add_data(value, "my_data");

        xxas::assert(vaddr > 0, "data vaddr > 0");

        auto sym_addr = assembler.get_symbol_address("my_data");
        xxas::assert(sym_addr.has_value(), "my_data symbol found");
        xxas::assert_eq(*sym_addr, vaddr);
    };

    // BSS reservation.
    void reserve_bss_section()
    {
        Assembler assembler{};

        auto vaddr = assembler.reserve_bss(1024, "heap");

        xxas::assert(vaddr > 0, "bss vaddr > 0");

        auto sym_addr = assembler.get_symbol_address("heap");
        xxas::assert(sym_addr.has_value(), "heap symbol found");
        xxas::assert_eq(*sym_addr, vaddr);
    };

    // Entry point set by symbol name.
    void entry_point_by_symbol()
    {
        Assembler assembler{};

        std::vector<std::byte> code{std::byte{0xCC}};
        assembler.add_code(std::span<const std::byte>{code.data(), code.size()}, "main");
        assembler.set_entry_point("main");

        auto build_result = assembler.build();
        xxas::assert(build_result.has_value(), "build_result.has_value()");

        auto main_addr = assembler.get_symbol_address("main");
        xxas::assert(main_addr.has_value(), "main symbol found");
        xxas::assert_eq(build_result->header.entry_point, *main_addr);
    };

    // Entry point set by raw address.
    void entry_point_by_address()
    {
        Assembler assembler{};

        std::vector<std::byte> code{std::byte{0xCC}};
        assembler.add_code(std::span<const std::byte>{code.data(), code.size()});
        assembler.set_entry_point(std::uint64_t{0x4000});

        auto build_result = assembler.build();
        xxas::assert(build_result.has_value(), "build_result.has_value()");
        xxas::assert_eq(build_result->header.entry_point, std::uint64_t{0x4000});
    };

    // Looking up a nonexistent symbol should return nullopt.
    void symbol_not_found()
    {
        Assembler assembler{};

        auto result = assembler.get_symbol_address("nonexistent");
        xxas::assert(!result.has_value(), "nonexistent symbol returns nullopt");
    };

    // Build with all four section types present.
    void build_all_sections()
    {
        Assembler assembler{};

        std::vector<std::byte> code{std::byte{0x90}, std::byte{0xC3}};
        assembler.add_code(std::span<const std::byte>{code.data(), code.size()}, "start");
        assembler.set_entry_point("start");

        assembler.add_string("readonly", "ro_str");
        assembler.add_data(std::uint32_t{42}, "init_val");
        assembler.reserve_bss(128, "uninit_buf");

        auto build_result = assembler.build();
        xxas::assert(build_result.has_value(), "build with all sections");

        auto& binary = *build_result;

        // Should have 4 sections.
        xxas::assert_eq(binary.header.section_count, std::uint32_t{4});
        xxas::assert(binary.header.is_valid(), "built header is valid");

        // All section types should be findable.
        xxas::assert(binary.find_section(Binary::Type::Code).has_value(),   "code section");
        xxas::assert(binary.find_section(Binary::Type::RoData).has_value(), "rodata section");
        xxas::assert(binary.find_section(Binary::Type::Data).has_value(),   "data section");
        xxas::assert(binary.find_section(Binary::Type::Bss).has_value(),    "bss section");
    };

    // Reset should clear all state.
    void assembler_reset()
    {
        Assembler assembler{};

        std::vector<std::byte> code{std::byte{0xCC}};
        assembler.add_code(std::span<const std::byte>{code.data(), code.size()}, "fn");
        assembler.add_string("data", "label");

        // Verify symbols exist.
        xxas::assert(assembler.get_symbol_address("fn").has_value(), "fn exists before reset");

        assembler.reset();

        // After reset, symbols should be gone.
        xxas::assert(!assembler.get_symbol_address("fn").has_value(),    "fn gone after reset");
        xxas::assert(!assembler.get_symbol_address("label").has_value(), "label gone after reset");

        // Should be able to build an empty binary (no sections).
        auto build_result = assembler.build();
        xxas::assert(build_result.has_value(), "empty build after reset");
        xxas::assert_eq(build_result->header.section_count, std::uint32_t{0});
    };

    // Multiple code additions should accumulate within the same section.
    void multiple_code_additions()
    {
        Assembler assembler{};

        std::vector<std::byte> prologue{std::byte{0x55}, std::byte{0x48}, std::byte{0x89}, std::byte{0xE5}};
        std::vector<std::byte> body{std::byte{0x90}, std::byte{0x90}};
        std::vector<std::byte> epilogue{std::byte{0x5D}, std::byte{0xC3}};

        auto addr_prologue = assembler.add_code(std::span<const std::byte>{prologue.data(), prologue.size()}, "prologue");
        auto addr_body     = assembler.add_code(std::span<const std::byte>{body.data(), body.size()}, "body");
        auto addr_epilogue = assembler.add_code(std::span<const std::byte>{epilogue.data(), epilogue.size()}, "epilogue");

        // Addresses should be ordered.
        xxas::assert(addr_body     > addr_prologue, "body after prologue");
        xxas::assert(addr_epilogue > addr_body,     "epilogue after body");

        // Build should produce exactly one code section.
        assembler.set_entry_point("prologue");
        auto build_result = assembler.build();
        xxas::assert(build_result.has_value(), "build_result.has_value()");

        auto code_idx = build_result->find_section(Binary::Type::Code);
        xxas::assert(code_idx.has_value(), "single code section exists");

        auto code_bytes = build_result->section_bytes(*code_idx);
        xxas::assert(code_bytes.has_value(), "code bytes retrievable");

        // Total code size should include alignment padding + all three blobs.
        xxas::assert(code_bytes->size() >= prologue.size() + body.size() + epilogue.size(),
            "code section contains all additions");
    };

    constexpr xxas::Tests assembler
    {
        vaddr_allocation,
        add_code_bytes,
        add_code_vector,
        add_string_constant,
        rodata_deduplication,
        add_initialized_data,
        reserve_bss_section,
        entry_point_by_symbol,
        entry_point_by_address,
        symbol_not_found,
        build_all_sections,
        assembler_reset,
        multiple_code_additions,
    };
};

int main()
{
    return mint_tests::assembler();
};
