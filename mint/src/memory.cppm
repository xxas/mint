export module mint: memory;

import std;
import xxas;

/*** **
 **
 **  module:   mint: memory
 **  purpose:  Provides cross-thread friendly emulation of virtual to physical mapping,
 **            page protection, allocation management, read, and writing to memory.
 **
 *** **/

namespace mint
{   
    // Memory related details.
    namespace mem
    {   
        // Page protection flags.
        export enum class Protection: std::uint8_t
        {
            Readable    = 0b001,
            Writtable   = 0b010,
            Executable  = 0b100,

            Rw  = 0b011,
            Rwe = 0b111,
        };

        constexpr Protection operator|(Protection a, Protection b)
        {
            return static_cast<Protection>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
        };

        constexpr bool operator&(Protection a, Protection b)
        {
            return static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b);
        };

        export struct Region
        {
            std::size_t     paddr;
            std::size_t     size;
            mem::Protection flags;
        };

        // Default size of a memory page.
        export constexpr inline std::size_t default_page_size = 0x1000;
    };

    export struct Memory
    {   // Virtual memory regions.
        using MemoryMap   = std::unordered_map<std::size_t, mem::Region>;

        // Physical memory bytes.
        using PhysicalMem = std::vector<std::byte>;

        MemoryMap                vmap;
        PhysicalMem              pmem;
        std::atomic<std::size_t> next_vaddr = 0x1000;
        std::size_t              page_len = mem::default_page_size;
        std::mutex               mem_mutex;

        enum class MemErrs: std::uint8_t
        {
            Alloc, VAddr, Stack, Access
        };

        using MemErr                      = xxas::Error<MemErrs>;
        template<class T> using MemResult = std::expected<T, MemErr>;

        // Align a virtual address to page boundaries.
        constexpr auto align(std::size_t virtual_addr) const noexcept
            -> std::size_t
        {
            return (virtual_addr + this->page_len - 1) & ~(this->page_len - 1);
        };

        // allocate a size of memory with flags and returns a virtual address corresponding.
        auto alloc(std::size_t size, mem::Protection flags = mem::Protection::Rw)
            -> MemResult<std::size_t>
        {
            if(size == 0uz)
            {
                return MemErr::err(MemErrs::Alloc, "Invalid allocation: size must be nonzero.");
            }

            size = this->align(size);

            std::scoped_lock lock(mem_mutex);  // Lock for thread safety.

            std::size_t paddr = this->pmem.size();
            this->pmem.resize(paddr + size);

            std::size_t vaddr = this->align(this->next_vaddr);
            this->next_vaddr  = vaddr + size;

            this->vmap[vaddr] = mem::Region{ paddr, size, flags };

            return vaddr;
        };

        // Translate virtual address to physical address.
        auto translate(std::size_t vaddr)
            -> MemResult<std::size_t>
        {
            std::scoped_lock lock(mem_mutex);

            std::size_t vaddr_base = this->align(vaddr);

            auto it = this->vmap.find(vaddr_base);
            if (it == this->vmap.end())
            {
                return MemErr::err(MemErrs::VAddr, "Invalid virtual address.");
            }

            std::size_t offset = vaddr - vaddr_base;
            if (offset >= it->second.size)
            {
                return MemErr::err(MemErrs::VAddr, "Address out of bounds.");
            }

            return it->second.paddr + offset;
        };

        // Read from virtual address and size.
        auto read(std::size_t vaddr, std::size_t size)
            -> std::expected<std::span<std::byte>, MemErr>
        {
            std::scoped_lock lock(mem_mutex);

            auto result = this->translate(vaddr);
            if (!result)
            {
                return MemErr::from(result);
            };

            auto paddr = *result;

            auto region_it = vmap.find(align(vaddr));
            if (region_it == vmap.end() || (region_it->second.flags & mem::Protection::Readable))
            {
                return MemErr::err(MemErrs::Access, "Memory read access denied.");
            };

            return std::span<std::byte>
            {
                &this->pmem[paddr], size
            };
        };

        // Write memory from a span
        auto write(std::size_t vaddr, std::span<const std::byte> bytes)
            -> std::expected<void, MemErr>
        {
            std::scoped_lock lock(mem_mutex);

            auto result = this->translate(vaddr);
            if (!result)
            {
                return MemErr::from(result);
            }

            auto paddr = *result;

            auto region_it = vmap.find(align(vaddr));
            if (region_it == vmap.end() || !(region_it->second.flags & mem::Protection::Writtable))
            {
                return MemErr::err(MemErrs::Access, "Memory write access denied.");
            }

            std::memcpy(&this->pmem[paddr], bytes.data(), bytes.size());

            return {};
        };
    };
};
