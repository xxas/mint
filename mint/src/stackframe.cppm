export module mint: stackframe;

import std;
import xxas;
import :memory;

/*** **
 **
 **  module:   mint: stackframe
 **  purpose:  Manages the stackframe for each function.
 **
 *** **/

namespace mint
{
    namespace stack
    {   // Default size of the stack.
        export constexpr inline std::size_t default_size = 0x10000;
    };

    export struct StackFrame
    {   // Stack pointer (points to the top of the stack).
        std::size_t sp;

        // Base pointer (frame start).
        std::size_t bp;

        // Virtual address of the allocated stack.
        std::size_t vaddr;

        // Stack size.
        std::size_t size;

        enum class Err: std::uint8_t
        {
            Address,
            Overflow,
            Underflow,
        };

        template<class T = std::void_t<>> using Result = xxas::Result<T, Err, Memory::Err>;

        StackFrame(std::size_t vaddr, std::size_t size)
            : sp(vaddr + size), bp(sp), vaddr(vaddr), size(size) {};

        // Ensures proper alignment for stack operations.
        constexpr auto align(std::size_t address, std::size_t alignment) const noexcept
            -> std::size_t
        {
            return (alignment == 0) ? address : (address & ~(alignment - 1));
        };

        // Push a raw slice of bytes onto the stack.
        template<std::ranges::contiguous_range T> auto push_bytes(Memory& mem, const T& data)
            -> Result<>
        {   // Ensure the data isn't larger than the stack.
            if(data.size() > this->sp - this->vaddr)
            {
                return xxas::error(Err::Overflow, "Stack overflow: Not enough space to push data.");
            };

            auto data_size = std::ranges::size(data) * sizeof(std::ranges::range_value_t<T>);

            this->sp -= data_size;

            auto slice_result = mem.slice(this->sp, data_size);
            if(!slice_result)
            {
                return slice_result.error();
            };

            if(auto copy_result = slice_result->copy(data); copy_result != 0u)
            {
                return xxas::error(Err::Address, std::format("Failed to copy {} bytes to memory.", copy_result));
            };

            return {};
        };

        // Pop a raw slice of bytes from the stack.
        auto pop_bytes(Memory& mem, std::size_t size)
            -> Result<std::vector<std::byte>>
        {
            if(this->sp + size > this->vaddr + this->size)
            {
                return xxas::error(Err::Underflow, "Stack underflow: Attempted to pop beyond allocated stack.");
            };

            auto slice_result = mem.slice(this->sp, size);
            if(!slice_result)
            {
                return slice_result.error();
            };

            std::vector<std::byte> data{size};
            if(auto copy_result = slice_result->clone(data); copy_result != 0u)
            {
                return xxas::error(Err::Address, std::format("Failed to copy {} bytes to memory.", copy_result));
            };

            // Increment the stack pointer back to the top.
            this->sp += size;

            return data;
        };

        // Push generic data onto the stack.
        template<class T> auto push(Memory& mem, const T& value)
            -> Result<>
        {
            constexpr std::size_t alignment = alignof(T);
            this->sp = align(this->sp - sizeof(T), alignment);

            std::span<const std::byte> data
            {
                reinterpret_cast<const std::byte*>(&value), sizeof(T)
            };

            return this->push_bytes(mem, data);
        };

        // Pop generic data from the stack safely.
        template<class T> auto pop(Memory& mem)
            -> Result<T>
        {
            constexpr std::size_t alignment = alignof(T);
            this->sp = this->align(this->sp, alignment);

            // Check stack underflow.
            if(this->sp + sizeof(T) > this->vaddr + this->size)
            {
                return xxas::error(Err::Underflow, "Stack underflow: Attempted to pop beyond allocated stack.");
            };

            if(auto result = this->pop_bytes(mem, sizeof(T)); !result)
            {
                return result.error();
            }
            else
            {
                return *reinterpret_cast<T*>(result->data());
            };
        };

        // Function prologue: save caller's state.
        auto function_prologue(Memory& mem)
            -> Result<>
        {   // Save previous base pointer.
            return this->push(mem, this->bp).and_then([this, &mem]
            {   // Update base pointer.
                this->bp = this->sp;
            });
        };

        // Function epilogue: restore caller's state.
        auto function_epilogue(Memory& mem)
            -> Result<>
        {   // Reset stack pointer.
            this->sp = this->bp;

            return this->pop<std::size_t>(mem).and_then([this](std::size_t old_bp)
              {   // Restore previous base pointer.
                  this->bp = old_bp;
              });
        };
    };
};

