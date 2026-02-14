export module mint: binary;

import std;
import xxas;

namespace mint
{
    namespace bin
    {   // Mint magic header.
        export constexpr std::uint32_t Magic   = xxas::fnv1a_32("Mint_cxx");
        export constexpr std::uint32_t Version = 0x00000001;

        // Max section count.
        export constexpr std::uint32_t MaxSections = 65536;

        export struct Header
        {
            std::uint32_t magic;
            std::uint32_t version;
            std::uint64_t entry_point;
            std::uint32_t section_count;

            constexpr Header() = default;
            constexpr Header(std::uint64_t entry_point, std::uint32_t section_count)
                : magic{ bin::Magic }, version{ bin::Version }, entry_point{ entry_point }, section_count{ section_count } {};

            // Validate header contents.
            constexpr auto is_valid() const noexcept
                -> bool
            {
                return this->magic == bin::Magic && this->version == bin::Version && this->section_count <= bin::MaxSections;
            };
        };

        export struct Section
        {
            enum class Type: std::uint32_t
            {
                Code = 0,   // Instructions.
                RoData,     // Constants; read only data.
                Data,       // Initialized data.
                Bss,        // Uninitialized data.
                Symbol,     // Symbol table.
            };

            std::uint32_t type;
            std::uint32_t size;
            std::uint64_t offset;
            std::uint64_t vaddr;

            constexpr Section() = default;
            constexpr Section(Type type, std::uint32_t size, std::uint64_t offset, std::uint64_t vaddr)
                : type{ std::to_underlying(type) }, size{ size }, offset{ offset }, vaddr{ vaddr } {};

            // Validate section doesn't overflow file.
            constexpr auto is_valid(std::size_t file_size) const noexcept
                -> bool
            {
                return this->offset <= file_size && this->size <= file_size && this->offset <= file_size - this->size;
            };
        };
    };

    export struct Binary
    {
        using Type       = bin::Section::Type;
        using SectionVec = std::vector<bin::Section>;
        using BytesVec   = std::vector<std::byte>;

        bin::Header header;
        SectionVec  sections;
        BytesVec    bytes;


        enum class Err: std::uint8_t
        {
            File,
            Header,
            Version,
            Sections,
            EntryPoint,
            OutOfBounds,
        };

        constexpr Binary(bin::Header header, SectionVec sections, BytesVec bytes)
            : header{ std::move(header) }, sections{ std::move(sections) }, bytes{ std::move(bytes) } {};

        // Deserialize from byte range.
        static constexpr auto from(const std::ranges::contiguous_range auto& range)
            -> xxas::Result<Binary, Err>
        {
            const auto bytes = std::span<const std::byte>{ std::ranges::data(range), std::ranges::size(range) };

            // Check minimum file size for header.
            if(bytes.size() < sizeof(bin::Header))
            {
                return xxas::error(Err::File, "File too small for header");
            };

            xxas::Deserializer deserializer
            {
               bytes
            };

            // Deserialize header.
            auto header_opt = deserializer.read<bin::Header>();
            if(!header_opt)
            {
                return xxas::error(Err::File, "Failed to read header");
            };

            auto header = *header_opt;

            if(header.magic != bin::Magic)
            {
                return xxas::error(Err::Header, "Bad header magic");
            };

            if(header.version != bin::Version)
            {
                return xxas::error(Err::Version, "Mismatched header version");
            };

            if(header.section_count > bin::MaxSections)
            {
                return xxas::error(Err::Sections, "Too many sections");
            };

            // Check if we have enough bytes for all section headers.
            const auto sections_size        = sizeof(bin::Section) * header.section_count;
            const auto header_and_sections  = sizeof(bin::Header) + sections_size;

            if(bytes.size() < header_and_sections)
            {
                return xxas::error(Err::Sections, "Not enough bytes for section headers");
            };

            // Deserialize sections.
            SectionVec sections;
            sections.reserve(header.section_count);

            for(std::uint32_t i = 0; i < header.section_count; ++i)
            {
                auto section_opt = deserializer.read<bin::Section>();
                if(!section_opt)
                {
                    return xxas::error(Err::Sections, std::format("Failed to read section {}", i));
                };

                auto section = *section_opt;

                // Validate section bounds.
                if(!section.is_valid(bytes.size()))
                {
                    return xxas::error(Err::OutOfBounds, std::format("Section {} out of bounds", i));
                };

                sections.push_back(section);
            };

            // Validate entry point is in a code section.
            const auto entry_valid = std::ranges::any_of(sections, [&](const auto& section)
            {
                return section.type == std::to_underlying(Type::Code) && header.entry_point >= section.vaddr
                    && header.entry_point < section.vaddr + section.size;
            });

            if(!entry_valid && header.section_count > 0)
            {
                return xxas::error(Err::EntryPoint, "Entry point is not within a code section");
            };

            // Copy all bytes.
            return Binary
            {
                std::move(header), std::move(sections), BytesVec(bytes.begin(), bytes.end())
            };
        };

        // Serialize to bytes.
        constexpr auto to_bytes() const
            -> BytesVec
        {
            xxas::Serializer serializer;
            serializer.bytes.reserve(this->bytes.size());

            // Serialize header.
            serializer.write(this->header);

            // Serialize sections.
            for(const auto& section: this->sections)
            {
                serializer.write(section);
            };

            // Copy remaining bytes (section data).
            const auto data_start = sizeof(bin::Header) + (sizeof(bin::Section) * this->header.section_count);

            if(data_start < this->bytes.size())
            {
                const auto remaining = std::span<const std::byte>
                {
                    this->bytes.data() + data_start,
                    this->bytes.size() - data_start
                };
                serializer.append(remaining);
            };

            return std::move(serializer.bytes);
        };

        // Get section data by index.
        constexpr auto section_bytes(const std::size_t index) const
            -> std::optional<std::span<const std::byte>>
        {
            if(index >= this->sections.size())
            {
                return std::nullopt;
            };

            const auto& section = this->sections[index];

            if(!section.is_valid(this->bytes.size()))
            {
                return std::nullopt;
            };

            return std::span<const std::byte>
            {
                this->bytes.data() + section.offset, section.size
            };
        };

        // Find section by type.
        constexpr auto find_section(const Type type) const
            -> std::optional<std::size_t>
        {
            const auto underlying = std::to_underlying(type);

            for(std::size_t i = 0; i < this->sections.size(); ++i)
            {
                if(this->sections[i].type == underlying)
                {
                    return i;
                };
            };

            return std::nullopt;
        };
    };
};
