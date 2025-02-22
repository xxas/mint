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
{   // Memory related details.
    namespace mem
    {   // Page protection flags.
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

        constexpr Protection operator&(Protection a, Protection b)
        {
            return static_cast<Protection>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
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
        using PhysicalMem = std::vector<std::uint8_t>;

        MemoryMap   vmap;
        PhysicalMem pmem;
        std::size_t next_vaddr  = 0x1000;
        std::size_t page_len    = mem::default_page_size;

        enum class MemErrs: std::uint8_t
        {
            Alloc, VAddr, Stack,
        };

        using MemErr    = xxas::Error<MemErrs>;
        using MemResult = std::expected<std::size_t, MemErr>;

        // Align a virtual address to page boundries.
        constexpr auto align(std::size_t virtual_addr) const noexcept
            -> std::size_t
        {
            return (virtual_addr + this->page_len - 1) & ~(this->page_len - 1);
        };

        auto alloc(std::size_t size, mem::Protection flags = mem::Protection::Rw)
            -> MemResult
        {   // Error on invalid allocations.
            if(size == 0uz)
            {
                return MemErr::err(MemErrs::Alloc, std::format("Invalid allocation with size of {}", size));
            };

            // Align the size of the allocation.
            size = this->align(size);

            // Get the physical address of the allocation.
            const std::size_t paddr = this->pmem.size();

            // Allocate the physical memory.
            this->pmem.resize(paddr + size);

            // Assign virtual address for the physical.
            const std::size_t vaddr = this->align(this->next_vaddr);
            this->next_vaddr        = vaddr + size;

            // Create the memory region mapping.
            this->vmap[vaddr] = mem::Region
            {
                paddr, size, flags
            };

            // Return the virtual address.
            return vaddr;
        };

        auto translate(std::size_t vaddr) const
            -> MemResult
        {   // Align the virtual address to page boundries.
            std::size_t vaddr_base = this->align(vaddr);

            // Find the allocated page for the aligned address.
            auto it = this->vmap.find(vaddr_base);

            // Invalid virtual address mapping.
            if(it == this->vmap.end())
            {
                return MemErr::err(MemErrs::VAddr, std::format("Invalid virtual address of {}", vaddr));
            };

            // Calculate the offset from the base address.
            std::size_t offset = vaddr - vaddr_base;

            // Return the physical address.
            return it->second.paddr + offset;
        };
    };


    namespace mem
    {
        // Default size of the stack.
        export constexpr inline std::size_t default_stack_size = 0x10000;

        export struct StackFrame
        {
            std::size_t sp;    // Stack pointer (points to the top of the stack)
            std::size_t bp;    // Base pointer (frame start)
            std::size_t vaddr; // Virtual address of the allocated stack
            std::size_t size;  // Stack size

            enum class StackErrs : std::uint8_t
            {
                Address,
                Overflow,
                Underflow,
            };

            using StackErr                                      = xxas::Error<StackErrs>;
            template<class T = std::void_t<>> using StackResult = std::expected<T, StackErr>;

            StackFrame(std::size_t vaddr, std::size_t size)
                : sp(vaddr + size), bp(sp), vaddr(vaddr), size(size) {}

            // Ensure proper alignment for stack operations.
            constexpr std::size_t align(std::size_t address, std::size_t alignment) const noexcept
            {
                return (address & ~(alignment - 1)); // Align to the nearest lower multiple
            }

            // Push a raw slice of bytes onto the stack.
            auto push_bytes(Memory& mem, std::span<const std::byte> data) -> StackResult<>
            {
                if (data.size() > sp - vaddr)
                {
                    return StackErr::err(StackErrs::Overflow, "Stack overflow: Not enough space to push data.");
                }

                // Move stack pointer down (aligned)
                sp -= data.size();

                auto result = mem.translate(sp);
                if (result.has_value())
                {
                    std::memcpy(&mem.pmem[result.value()], data.data(), data.size());
                    return {};
                }

                return StackErr::err(StackErrs::Address, "Invalid address translation in push_bytes()");
            }

            // Pop a raw slice of bytes from the stack.
            auto pop_bytes(Memory& mem, std::span<std::byte> data) -> StackResult<>
            {
                if (sp + data.size() > vaddr + size)
                {
                    return StackErr::err(StackErrs::Underflow, "Stack underflow: Attempted to pop beyond allocated stack.");
                }

                auto result = mem.translate(sp);
                if (result.has_value())
                {
                    std::memcpy(data.data(), &mem.pmem[result.value()], data.size());
                    sp += data.size();  // Move stack pointer up
                    return {};
                }

                return StackErr::err(StackErrs::Address, "Invalid address translation in pop_bytes()");
            }

            // Push generic data onto the stack.
            template<class T> 
            auto push(Memory& mem, const T& value) -> StackResult<>
            {
                constexpr std::size_t alignment = alignof(T);
                sp = align(sp - sizeof(T), alignment); // Ensure proper alignment before writing

                std::span<const std::byte> data{
                    reinterpret_cast<const std::byte*>(&value), sizeof(T)
                };

                return this->push_bytes(mem, data);
            }

            // Pop generic data from the stack safely.
            template<class T> 
            auto pop(Memory& mem) -> StackResult<T>
            {
                constexpr std::size_t alignment = alignof(T);
                sp = align(sp, alignment); // Ensure proper alignment before reading

                if (sp + sizeof(T) > vaddr + size) // Check stack underflow
                {
                    return StackErr::err(StackErrs::Underflow, "Stack underflow: Attempted to pop beyond allocated stack.");
                }

                T value{};
                std::span<std::byte> data{
                    reinterpret_cast<std::byte*>(&value), sizeof(T)
                };

                auto result = this->pop_bytes(mem, data);
                if (!result.has_value())
                {
                    return StackErr::err(StackErrs::Address, "Failed to pop data from the stack.");
                }

                return value;
            };

            // Function prologue: save caller's state
            auto function_prologue(Memory& mem) -> StackResult<>
            {
                return push(mem, bp) // Save previous base pointer
                    .and_then([this, &mem]() -> StackResult<> {
                        bp = sp;  // Update base pointer
                        return {};
                    });
            }

            // Function epilogue: restore caller's state
            auto function_epilogue(Memory& mem) -> StackResult<>
            {
                sp = bp;  // Reset stack pointer
                return pop<std::size_t>(mem).and_then([this](std::size_t old_bp) -> StackResult<> {
                    bp = old_bp;  // Restore previous base pointer
                    return {};
                });
            }
        };
    };
};
