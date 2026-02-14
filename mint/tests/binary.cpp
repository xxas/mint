import std;
import xxas;
import mint;

namespace mint_tests
{
    using namespace mint;

    // Verify that a correctly constructed header passes validation.
    constexpr void header_valid()
    {
        auto header = bin::Header(0x1000, 2);

        xxas::assert(header.is_valid(), "header.is_valid()");
        xxas::assert_eq(header.magic, bin::Magic);
        xxas::assert_eq(header.version, bin::Version);
        xxas::assert_eq(header.entry_point, std::uint64_t{0x1000});
        xxas::assert_eq(header.section_count, std::uint32_t{2});
    };

    // A header with bad magic should fail validation.
    constexpr void header_bad_magic()
    {
        auto header = bin::Header(0x1000, 1);
        header.magic = 0xDEADBEEF;

        xxas::assert(!header.is_valid(), "!header.is_valid() after bad magic");
    };

    // A header with too many sections should fail validation.
    constexpr void header_too_many_sections()
    {
        auto header = bin::Header(0x0, bin::MaxSections + 1);

        xxas::assert(!header.is_valid(), "!header.is_valid() with too many sections");
    };

    // Section bounds checking against a file size.
    constexpr void section_bounds()
    {
        auto section = bin::Section(bin::Section::Type::Code, 64, 128, 0x2000);

        // File is large enough.
        xxas::assert(section.is_valid(256), "section.is_valid(256)");

        // File is too small for section data.
        xxas::assert(!section.is_valid(100), "!section.is_valid(100)");

        // File is exactly the right size.
        xxas::assert(section.is_valid(192), "section.is_valid(192)");
    };

    // Build a binary through the assembler and round-trip it.
    void roundtrip_serialization()
    {
        Assembler assembler{};

        // Add a code section with some placeholder bytes.
        std::vector<std::byte> code
        {
            std::byte{0x90}, std::byte{0x90}, std::byte{0xCC}, std::byte{0xC3},
            std::byte{0x48}, std::byte{0x89}, std::byte{0xE5}, std::byte{0xC3},
        };

        auto code_vaddr = assembler.add_code(std::span<const std::byte>{code.data(), code.size()}, "entry");
        assembler.set_entry_point("entry");

        // Add a rodata section with a string.
        assembler.add_string("hello world", "greeting");

        // Add an initialized data section.
        std::uint64_t global_counter = 42;
        assembler.add_data(global_counter, "counter");

        // Reserve some bss.
        assembler.reserve_bss(256, "buffer");

        // Build the binary.
        auto build_result = assembler.build();
        xxas::assert(build_result.has_value(), "build_result.has_value()");

        auto& original = *build_result;

        // Serialize to bytes.
        auto bytes = original.to_bytes();
        xxas::assert(bytes.size() > sizeof(bin::Header), "bytes.size() > header size");

        // Deserialize from the serialized bytes.
        auto deserialized = Binary::from(bytes);
        xxas::assert(deserialized.has_value(), "deserialized.has_value()");

        // Validate header fields match.
        xxas::assert_eq(deserialized->header.magic,         original.header.magic);
        xxas::assert_eq(deserialized->header.version,       original.header.version);
        xxas::assert_eq(deserialized->header.entry_point,   original.header.entry_point);
        xxas::assert_eq(deserialized->header.section_count, original.header.section_count);

        // Validate section count matches.
        xxas::assert_eq(deserialized->sections.size(), original.sections.size());
    };

    // Deserializing an empty buffer should fail.
    void deserialize_empty()
    {
        std::vector<std::byte> empty{};

        auto result = Binary::from(empty);
        xxas::assert(!result.has_value(), "!result.has_value() for empty input");
    };

    // Deserializing a buffer that is too small for the header should fail.
    void deserialize_truncated_header()
    {
        std::vector<std::byte> small(sizeof(bin::Header) - 1, std::byte{0});

        auto result = Binary::from(small);
        xxas::assert(!result.has_value(), "!result.has_value() for truncated header");
    };

    // Deserializing a buffer with bad magic should fail.
    void deserialize_bad_magic()
    {
        // Create a valid binary then corrupt the magic.
        Assembler assembler{};

        std::vector<std::byte> code{std::byte{0x90}};
        assembler.add_code(std::span<const std::byte>{code.data(), code.size()});

        auto build_result = assembler.build();
        xxas::assert(build_result.has_value(), "build_result.has_value()");

        auto bytes = build_result->to_bytes();

        // Corrupt magic (first 4 bytes).
        bytes[0] = std::byte{0xFF};
        bytes[1] = std::byte{0xFF};

        auto result = Binary::from(bytes);
        xxas::assert(!result.has_value(), "!result.has_value() for bad magic");
    };

    // find_section should locate sections by type.
    void find_section_by_type()
    {
        Assembler assembler{};

        std::vector<std::byte> code{std::byte{0xCC}};
        assembler.add_code(std::span<const std::byte>{code.data(), code.size()});
        assembler.add_string("test", "label");
        assembler.reserve_bss(64, "uninit");

        auto build_result = assembler.build();
        xxas::assert(build_result.has_value(), "build_result.has_value()");

        auto& binary = *build_result;

        // Code section should exist.
        auto code_idx = binary.find_section(Binary::Type::Code);
        xxas::assert(code_idx.has_value(), "code section found");

        // RoData section should exist.
        auto rodata_idx = binary.find_section(Binary::Type::RoData);
        xxas::assert(rodata_idx.has_value(), "rodata section found");

        // Bss section should exist.
        auto bss_idx = binary.find_section(Binary::Type::Bss);
        xxas::assert(bss_idx.has_value(), "bss section found");

        // section_bytes should return valid spans.
        auto code_bytes = binary.section_bytes(*code_idx);
        xxas::assert(code_bytes.has_value(), "code_bytes.has_value()");
        xxas::assert(code_bytes->size() > 0, "code_bytes not empty");
    };

    // section_bytes out of bounds should return nullopt.
    void section_bytes_out_of_bounds()
    {
        Assembler assembler{};

        std::vector<std::byte> code{std::byte{0x90}};
        assembler.add_code(std::span<const std::byte>{code.data(), code.size()});

        auto build_result = assembler.build();
        xxas::assert(build_result.has_value(), "build_result.has_value()");

        auto result = build_result->section_bytes(999);
        xxas::assert(!result.has_value(), "!result.has_value() for out of bounds index");
    };

    constexpr xxas::Tests binary
    {
        header_valid,
        header_bad_magic,
        header_too_many_sections,
        section_bounds,
        roundtrip_serialization,
        deserialize_empty,
        deserialize_truncated_header,
        deserialize_bad_magic,
        find_section_by_type,
        section_bytes_out_of_bounds,
    };
};

int main()
{
    return mint_tests::binary();
};
