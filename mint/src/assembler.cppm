export module mint: assembler;

import std;
import xxas;
import :binary;

/*** **
 **
 **  module:   mint: assembler
 **  purpose:  Assembles low-level instructions, data into a singular binary.
 **
 *** **/

namespace mint
{
    namespace assembler
    {   // Symbol information.
        struct Symbol
        {
            std::string         name;
            std::uint64_t       address;
            std::uint32_t       size;
            bin::Section::Type  section_type;
            bool                is_global;

            constexpr Symbol(std::string name, std::uint64_t address, std::uint32_t size, bin::Section::Type type, bool is_global = false)
                : name{ std::move(name) }, address{ address }, size{ size }, section_type{ type }, is_global{ is_global } {};
        };

        // Relocation entry for linking.
        struct Relocation
        {
            enum class Type: std::uint8_t
            {
                Absolute64,                 // 64-bit absolute address.
                Relative32,                 // 32-bit PC-relative.
            };

            std::uint64_t   offset;         // Where to apply relocation.
            std::string     symbol_name;    // Symbol to relocate to.
            Type            type;
        };

        // Section builder helper.
        struct SectionBuilder
        {
            using Type      = bin::Section::Type;
            using Bytes     = std::vector<std::byte>;
            using RelocVec  = std::vector<Relocation>;

            Type          type;
            Bytes         data;
            std::uint64_t vaddr;
            RelocVec      relocations;

            constexpr SectionBuilder(Type type, std::uint64_t vaddr)
                : type{ type }, vaddr{ vaddr } {};

            // Append raw bytes.
            constexpr auto append(std::span<const std::byte> bytes)
                -> SectionBuilder&
            {
                this->data.insert(this->data.end(), bytes.begin(), bytes.end());
                return *this;
            };

            // Append typed data.
            template<class T> requires (!std::ranges::contiguous_range<T>)
            constexpr auto append(const T& value)
                -> SectionBuilder&
            {
                auto bytes = xxas::encode(value);
                this->data.insert(this->data.end(), bytes.begin(), bytes.end());
                return *this;
            };

            // Append range as bytes.
            constexpr auto append(const std::ranges::contiguous_range auto& range)
                -> SectionBuilder&
            {
                const auto bytes = std::as_bytes(std::span{ range });
                this->data.insert(this->data.end(), bytes.begin(), bytes.end());
                return *this;
            };

            // Get current size.
            constexpr auto size() const noexcept
                -> std::size_t
            {
                return this->data.size();
            };

            // Align to boundary.
            constexpr auto align(const std::size_t alignment)
                -> SectionBuilder&
            {
                const auto remainder = this->data.size() % alignment;
                if(remainder != 0)
                {
                    const auto padding = alignment - remainder;
                    this->data.insert(this->data.end(), padding, std::byte{ 0 });
                };
                return *this;
            };
        };

        // Data deduplication helper.
        struct DataPool
        {
            using Bytes   = std::vector<std::byte>;
            using HashMap = std::unordered_map<std::string_view, std::uint64_t>;

            Bytes   pool;
            HashMap hash_to_offset;

            // Add data, returning offset (deduplicating identical data).
            constexpr auto add(std::span<const std::byte> data)
                -> std::uint64_t
            {
                // Create view for dedup. check.
                const auto view = std::string_view
                {
                    reinterpret_cast<const char*>(data.data()),
                    data.size()
                };

                // Check if we already have this data.
                if(auto it = this->hash_to_offset.find(view); it != this->hash_to_offset.end())
                {
                    return it->second;
                };

                // New data. We should add it to the pool.
                const auto offset = this->pool.size();
                this->pool.insert(this->pool.end(), data.begin(), data.end());

                // Store persistent string.
                const auto persistent = std::string_view
                {
                    reinterpret_cast<const char*>(this->pool.data() + offset),
                    data.size()
                };
                this->hash_to_offset[persistent] = offset;

                return offset;
            };

            template<class T> constexpr auto add(const T& value)
                -> std::uint64_t
              requires std::is_trivially_copyable_v<T>
            {
                auto bytes = xxas::encode(value);
                return this->add(std::span<const std::byte>{ bytes.data(), bytes.size() });
            };
        };
    };

    export struct Assembler
    {
        using Bytes         = std::vector<std::byte>;
        using Type          = bin::Section::Type;
        using SymbolTable   = std::vector<assembler::Symbol>;
        using IndexMap      = std::unordered_map<std::string, std::size_t>;
        using OptSection    = std::optional<assembler::SectionBuilder>;

        // Virtual address allocation.
        std::uint64_t   next_vaddr  = 0x1000;
        std::uint64_t   entry_point = 0;
        bool            entry_set   = false;

        // Section builders.
        OptSection code_section;
        OptSection rodata_section;
        OptSection data_section;
        OptSection bss_section;

        // Symbol management.
        SymbolTable symbols;
        IndexMap    symbol_index;

        // Constant pool for deduplication.
        assembler::DataPool const_pool;

        constexpr Assembler() = default;

        // Allocate virtual address range.
        constexpr auto allocate_vaddr(std::size_t size, std::size_t alignment = 16)
            -> std::uint64_t
        {
            const auto remainder = this->next_vaddr % alignment;
            if(remainder != 0)
            {
                this->next_vaddr += alignment - remainder;
            };

            const auto vaddr = this->next_vaddr;
            this->next_vaddr += size;
            return vaddr;
        };

        // Get or create section.
        constexpr auto get_or_create_section(Type type)
            -> assembler::SectionBuilder&
        {
            auto& section = [&]()
                -> OptSection&
            {
                switch(type)
                {
                    case Type::Code:    return this->code_section;
                    case Type::RoData:  return this->rodata_section;
                    case Type::Data:    return this->data_section;
                    case Type::Bss:     return this->bss_section;

                    // This is unreachable; there is no way to have any other enum value passed in.
                    default:            std::unreachable();
                };
            }();

            if(!section)
            {
                section.emplace(type, this->allocate_vaddr(0));
            };

            return *section;
        };

        // Set entry point by address.
        constexpr auto set_entry_point(std::uint64_t address)
            -> Assembler&
        {
            this->entry_point = address;
            this->entry_set = true;
            return *this;
        };

        // Set entry point by symbol name.
        constexpr auto set_entry_point(const std::string& symbol_name)
            -> Assembler&
        {
            if(auto it = this->symbol_index.find(symbol_name); it != this->symbol_index.end())
            {
                this->entry_point = this->symbols[it->second].address;
                this->entry_set = true;
            };
            return *this;
        };

        // Add a symbol.
        constexpr auto add_symbol(std::string name, std::uint64_t address, std::uint32_t size, Type section_type, bool is_global = false)
            -> Assembler&
        {
            const auto index = this->symbols.size();
            this->symbol_index[name] = index;
            this->symbols.emplace_back(std::move(name), address, size, section_type, is_global);
            return *this;
        };

        // Add code from byte span.
        constexpr auto add_code(std::span<const std::byte> instructions, std::optional<std::string> label = std::nullopt)
            -> std::uint64_t
        {
            auto& section = this->get_or_create_section(Type::Code);
            section.align(16);

            const auto vaddr = section.vaddr + section.size();

            if(label)
            {
                this->add_symbol(*label, vaddr, static_cast<std::uint32_t>(instructions.size()), Type::Code, true);
            };

            section.append(instructions);

            // Advance the allocator past this section's content so that
            // sections created later don't overlap.
            this->next_vaddr = std::max(this->next_vaddr, section.vaddr + section.size());

            return vaddr;
        };

        // Add code from single instruction.
        template<class T> constexpr auto add_code(const T& instruction, std::optional<std::string> label = std::nullopt)
            -> std::uint64_t
          requires std::copyable<T>
        {
            auto bytes = xxas::encode(instruction);
            return this->add_code(std::span<const std::byte>{ bytes.data(), bytes.size() }, label);
        };

        // Add code from instruction vector.
        template<class T> constexpr auto add_code(const std::vector<T>& instructions, std::optional<std::string> label = std::nullopt)
            -> std::uint64_t
          requires std::copyable<T>
        {
            auto& section = this->get_or_create_section(Type::Code);
            section.align(16);

            const auto vaddr = section.vaddr + section.size();
            const auto byte_size = instructions.size() * sizeof(T);

            if(label)
            {
                this->add_symbol(*label, vaddr, static_cast<std::uint32_t>(byte_size), Type::Code, true);
            };

            section.append(instructions);
            this->next_vaddr = std::max(this->next_vaddr, section.vaddr + section.size());
            return vaddr;
        };

        // Add read-only data with optional deduplication.
        constexpr auto add_rodata(std::span<const std::byte> data, std::optional<std::string> label = std::nullopt, bool deduplicate = true)
            -> std::uint64_t
        {
            auto& section = this->get_or_create_section(Type::RoData);
            section.align(8);

            const auto vaddr = deduplicate
                ? section.vaddr + this->const_pool.add(data)
                : section.vaddr + section.size();

            if(!deduplicate)
            {
                section.append(data);
            };

            if(label)
            {
                this->add_symbol(*label, vaddr, static_cast<std::uint32_t>(data.size()), Type::RoData, true);
            };

            this->next_vaddr = std::max(this->next_vaddr, section.vaddr + section.size());

            return vaddr;
        };

        // Add constant with automatic deduplication.
        template<class T> constexpr auto add_constant(const T& value, std::optional<std::string> label = std::nullopt)
            -> std::uint64_t
          requires std::is_trivially_copyable_v<T>
        {
            auto bytes = xxas::encode(value);
            return this->add_rodata(std::span<const std::byte>{ bytes.data(), bytes.size() }, label, true);
        };

        // Add string constant with null termination and deduplication.
        constexpr auto add_string(std::string_view str, std::optional<std::string> label = std::nullopt)
            -> std::uint64_t
        {
            Bytes bytes;
            bytes.reserve(str.size() + 1);

            const auto str_bytes = std::bit_cast<const std::byte*>(str.data());
            bytes.insert(bytes.end(), str_bytes, str_bytes + str.size());
            bytes.push_back(std::byte{ 0 });

            return this->add_rodata(bytes, label, true);
        };

        // Add initialized data.
        constexpr auto add_data(std::span<const std::byte> data, std::optional<std::string> label = std::nullopt)
            -> std::uint64_t
        {
            auto& section = this->get_or_create_section(Type::Data);
            section.align(8);

            const auto vaddr = section.vaddr + section.size();

            if(label)
            {
                this->add_symbol(*label, vaddr, static_cast<std::uint32_t>(data.size()), Type::Data, true);
            };

            section.append(data);
            this->next_vaddr = std::max(this->next_vaddr, section.vaddr + section.size());
            return vaddr;
        };

        template<class T> constexpr auto add_data(const T& value, std::optional<std::string> label = std::nullopt)
            -> std::uint64_t
          requires std::is_trivially_copyable_v<T>
        {
            auto bytes = xxas::encode(value);
            return this->add_data(std::span<const std::byte>{ bytes.data(), bytes.size() }, label);
        };

        // Reserve uninitialized data (BSS).
        constexpr auto reserve_bss(std::uint32_t size, std::optional<std::string> label = std::nullopt)
            -> std::uint64_t
        {
            auto& section = this->get_or_create_section(Type::Bss);
            section.align(8);

            const auto vaddr = section.vaddr + section.size();

            if(label)
            {
                this->add_symbol(*label, vaddr, size, Type::Bss, true);
            };

            section.data.insert(section.data.end(), size, std::byte{ 0 });
            this->next_vaddr = std::max(this->next_vaddr, section.vaddr + section.size());
            return vaddr;
        };

        // Add relocation.
        constexpr auto add_relocation(Type section_type, std::uint64_t offset, std::string symbol_name, assembler::Relocation::Type reloc_type)
            -> Assembler&
        {
            auto& section = this->get_or_create_section(section_type);
            section.relocations.push_back({ offset, std::move(symbol_name), reloc_type });
            return *this;
        };

        // Get symbol address.
        constexpr auto get_symbol_address(const std::string& name) const
            -> std::optional<std::uint64_t>
        {
            if(auto it = this->symbol_index.find(name); it != this->symbol_index.end())
            {
                return this->symbols[it->second].address;
            };
            return std::nullopt;
        };

        // Build the final binary.
        constexpr auto build()
            -> xxas::Result<Binary, std::string>
        {
            // Merge constant pool into rodata section.
            if(!this->const_pool.pool.empty())
            {
                auto& section = this->get_or_create_section(Type::RoData);
                section.data = std::move(this->const_pool.pool);
            };

            xxas::Serializer serializer;
            std::vector<bin::Section> sections;

            const auto present =
                (this->code_section   ? 1u : 0u) +
                (this->rodata_section ? 1u : 0u) +
                (this->data_section   ? 1u : 0u) +
                (this->bss_section    ? 1u : 0u);

            std::uint64_t data_offset = sizeof(bin::Header) + (present * sizeof(bin::Section));
 
            auto process_section = [&](OptSection& builder) -> void
            {
                if(!builder) return;

                sections.emplace_back(
                    builder->type,
                    static_cast<std::uint32_t>(builder->size()),
                    data_offset,
                    builder->vaddr
                );

                data_offset += builder->size();
            };

            process_section(this->code_section);
            process_section(this->rodata_section);
            process_section(this->data_section);
            process_section(this->bss_section);

            // Set default entry point if not specified.
            if(!this->entry_set && this->code_section)
            {
                this->entry_point = this->code_section->vaddr;
            };

            // Build binary using serializer.
            bin::Header header
            {
                this->entry_point,
                static_cast<std::uint32_t>(sections.size())
            };

            serializer.write(header);

            for(const auto& section: sections)
            {
                serializer.write(section);
            };

            auto append_section_data = [&](OptSection& builder)
            {
                if(builder)
                {
                    serializer.append(builder->data);
                };
            };

            append_section_data(this->code_section);
            append_section_data(this->rodata_section);
            append_section_data(this->data_section);
            append_section_data(this->bss_section);

            return Binary
            {
                std::move(header),
                std::move(sections),
                std::move(serializer.bytes)
            };
        };

        // Clear and reset.
        constexpr auto reset()
            -> Assembler&
        {
            this->next_vaddr   = 0x1000;
            this->entry_point  = 0;
            this->entry_set    = false;

            this->code_section.reset();
            this->rodata_section.reset();
            this->data_section.reset();
            this->bss_section.reset();

            this->symbols.clear();
            this->symbol_index.clear();
            this->const_pool = {};

            return *this;
        };
    };
};
