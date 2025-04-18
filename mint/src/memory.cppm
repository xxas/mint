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

        // Atomic-safe next virtual address.
        using AtomicAddr  = std::atomic<std::size_t>;

        MemoryMap            vmap;
        PhysicalMem          pmem;
        AtomicAddr           next_vaddr = 0x1000;
        std::size_t          page_len   = mem::default_page_size;
        std::recursive_mutex mem_mutex;

        enum class Err: std::uint8_t
        {
            Alloc, VAddr, Stack, Access
        };

        template<class T> using Result = xxas::Result<T, Err>;

        // Align a virtual address to page boundaries.
        constexpr auto align(std::size_t virtual_addr) const noexcept
            -> std::size_t
        {
            return (virtual_addr + this->page_len - 1) & ~(this->page_len - 1);
        };

        // allocate a size of memory with flags and returns a virtual address corresponding.
        auto alloc(std::size_t size, mem::Protection flags = mem::Protection::Rw)
            -> Result<std::size_t>
        {
            if(size == 0uz)
            {
                return xxas::error(Err::Alloc, "Invalid allocation: size must be nonzero");
            }

            size = this->align(size);

            std::scoped_lock lock(this->mem_mutex);  // Lock for thread safety.

            std::size_t paddr = this->pmem.size();
            this->pmem.resize(paddr + size);

            std::size_t vaddr = this->align(this->next_vaddr);
            this->next_vaddr  = vaddr + size;

            this->vmap[vaddr] = mem::Region{ paddr, size, flags };

            return vaddr;
        };

        // Translate virtual address to physical address.
        auto translate(std::size_t vaddr)
            -> Result<std::size_t>
        {
            std::scoped_lock lock(this->mem_mutex);

            std::size_t vaddr_base = this->align(vaddr);

            auto it = this->vmap.find(vaddr_base);
            if (it == this->vmap.end())
            {
                return xxas::error(Err::VAddr, "Invalid virtual address");
            }

            std::size_t offset = vaddr - vaddr_base;
            if (offset >= it->second.size)
            {
                return xxas::error(Err::VAddr, "Address out of bounds");
            }

            return it->second.paddr + offset;
        };

        // Read from virtual address and size.
        auto read(std::size_t vaddr, std::size_t size)
            -> Result<std::span<std::byte>>
        {
            std::scoped_lock lock(this->mem_mutex);

            auto result = this->translate(vaddr);
            if (!result)
            {
                return result.error();
            };

            auto paddr = *result;

            auto region_it = vmap.find(this->align(vaddr));
            if (region_it == vmap.end() || !(region_it->second.flags & mem::Protection::Readable))
            {
                return xxas::error(Err::Access, "Memory read access denied");
            };

            return std::span<std::byte>
            {
                &this->pmem[paddr], size
            };
        };

        // Write memory from a span
        template<std::ranges::contiguous_range T> auto write(std::size_t vaddr, const T& range)
            -> Result<void>
        {
            std::scoped_lock lock(this->mem_mutex);

            auto result = this->translate(vaddr);
            if (!result)
            {
                return result.error();
            }

            auto paddr = *result;

            auto region_it = vmap.find(this->align(vaddr));
            if (region_it == vmap.end() || !(region_it->second.flags & mem::Protection::Writtable))
            {
                return xxas::error(Err::Access, "Memory write access denied");
            }

            std::memcpy(&this->pmem[paddr], std::ranges::data(range), std::ranges::size(range) * sizeof(std::ranges::range_value_t<T>));

            return {};
        };
    };
};
