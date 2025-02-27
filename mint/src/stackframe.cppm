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

        enum class StackErrs : std::uint8_t
        {
            Address,
            Overflow,
            Underflow,
        };

        using StackErr   = xxas::Error<StackErrs, Memory::MemErr>;
        template<class T = std::void_t<>> using StackResult = std::expected<T, StackErr>;

        StackFrame(std::size_t vaddr, std::size_t size)
            : sp(vaddr + size), bp(sp), vaddr(vaddr), size(size) {};

        // Ensures proper alignment for stack operations.
        constexpr auto align(std::size_t address, std::size_t alignment) const noexcept
            -> std::size_t
        {
            return (alignment == 0) ? address : (address & ~(alignment - 1));
        };

        // Push a raw slice of bytes onto the stack.
        auto push_bytes(Memory& mem, std::span<const std::byte> data)
            -> StackResult<>
        {
            if (data.size() > sp - vaddr)
            {
                return StackErr::err(StackErrs::Overflow, "Stack overflow: Not enough space to push data.");
            };

            this->sp -= data.size();

            auto result = mem.translate(sp);
            if(!result)
            {
                return StackErr::from(result);
            };

            auto paddr = *result;

            std::memcpy(&mem.pmem[result.value()], data.data(), data.size());

            return {};
        };

        // Pop a raw slice of bytes from the stack.
        auto pop_bytes(Memory& mem, std::span<std::byte> data)
            -> StackResult<>
        {
            if (this->sp + data.size() > this->vaddr + this->size)
            {
                return StackErr::err(StackErrs::Underflow, "Stack underflow: Attempted to pop beyond allocated stack.");
            }

            auto result = mem.translate(sp);
            if(!result)
            {
                return StackErr::from(result);
            };

            auto paddr = *result;

            std::memcpy(data.data(), &mem.pmem[paddr], data.size());
            this->sp += data.size();

            return {};
        };

        // Push generic data onto the stack.
        template<class T>  auto push(Memory& mem, const T& value)
            -> StackResult<>
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
            -> StackResult<T>
        {
            constexpr std::size_t alignment = alignof(T);
            this->sp = this->align(this->sp, alignment);

            // Check stack underflow
            if (this->sp + sizeof(T) > this->vaddr + this->size)
            {
                return StackErr::err(StackErrs::Underflow, "Stack underflow: Attempted to pop beyond allocated stack.");
            };

            T value{};
            std::span<std::byte> data
            {
                reinterpret_cast<std::byte*>(&value), sizeof(T)
            };

            auto result = this->pop_bytes(mem, data);
            if (!result)
            {
                return StackErr::err(StackErrs::Address, "Failed to pop data from the stack.");
            };

            return value;
        };

        // Function prologue: save caller's state
        auto function_prologue(Memory& mem)
            -> StackResult<>
        {   // Save previous base pointer.
            return push(mem, this->bp)
                .and_then([this, &mem]()
                    -> StackResult<>
                {   // Update base pointer.
                    this->bp = this->sp;
                    return {};
                });
        };

        // Function epilogue: restore caller's state.
        auto function_epilogue(Memory& mem)
            -> StackResult<>
        {   // Reset stack pointer.
            this->sp = this->bp;

            return pop<std::size_t>(mem)
              .and_then([this](std::size_t old_bp)
                  -> StackResult<>
              {   // Restore previous base pointer.
                  this->bp = old_bp;
                  return {};
              });
        };
    };
};

