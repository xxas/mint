module;
#include <experimental/simd>

export module mint: memory;

import std;
import xxas;

#if defined(_LIBCPP_EXPERIMENTAL_SIMD) || defined(_LIBCPP_SIMD)
  // Mint uses std::simd
  #ifndef MINT_SIMD
    #define MINT_SIMD 1
  #endif

  // Expose std::experimental::* -> std::*.
  namespace std{ using namespace experimental; };
#endif

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
        export enum class Flags: std::uint8_t
        {   // Manual flags.
            Read      = 0b001,
            Write     = 0b010,
            Execute   = 0b100,

            Rw        = 0b011,
            Re        = 0b101,
            Rwe       = 0b111,
            None      = 0b000,

            Default   = Rw,
        };

        export struct Page
        {
            using Mutex = std::shared_ptr<std::shared_mutex>;

            std::uintptr_t vaddr;
            std::size_t    size;
            Flags          flags;
            Mutex          mutex;

            constexpr Page(const std::uintptr_t vaddr, const std::size_t size, const Flags flags = Flags::Rw)
                :vaddr{vaddr}, size{size}, flags{flags}, mutex{std::make_shared<std::shared_mutex>()} {};

            // Returns if the address provided is within the pages bounds.
            constexpr auto contains(const std::uintptr_t vaddr)
            {
                return this->vaddr <= vaddr && (this->vaddr + this->size) >= vaddr;
            };
        };

        // TODO:
        // Thread-safe non-owning memory slice.
        /* template<class T> struct Slice
        {
            using Mutex = std::shared_ptr<std::shared_mutex>;
            using Span  = std::span<T>;

            Span  span;
            Mutex mutex;
        };*/

        // Thread-safe non-owning multiple page memory slice container.
        export template<class T> struct Shared
        {   // TODO:
            // Slice of memory within a page.
            // using Slices = std::vector<Slice<T>>;
            // Slices slices;

            using Mutex = std::shared_ptr<std::shared_mutex>;
            using Span  = std::span<T>;

            Span  span;
            Mutex mutex;

            template<class O> constexpr Shared<O> as() const
            {
                return Shared<O>
                {
                    .span = std::span<O>
                    {
                        reinterpret_cast<O*>(this->span.data()),
                        this->span.size_bytes() / sizeof(O)
                    },
                    .mutex = this->mutex
                };
            };

            template<class O = T> constexpr Shared<O> subrange(const std::size_t begin, const std::size_t end)
            {
                auto sub = span.subspan(begin, end - begin);
                return Shared<O>
                {
                    .span = std::span<O>(reinterpret_cast<O*>(sub.data()), sub.size_bytes() / sizeof(O)),
                    .mutex = this->mutex
                };
            };

            #ifdef MINT_SIMD
              // Convert the std::span to use std::native_simd.
              template<class O = T> constexpr auto par() const
                  -> Shared<std::native_simd<O>>
              {
                  using Simd        = std::native_simd<O>;

                  auto* data        = reinterpret_cast<Simd*>(this->span.data());
                  std::size_t count = this->span.size_bytes() / sizeof(Simd);

                  return Shared<Simd>
                  {
                      .span = std::span<Simd>(data, count),
                      .mutex = this->mutex
                  };
              };
            #endif

            template<class F> auto shared(F&& funct) const
            {
                std::shared_lock lock(*this->mutex);
                return std::invoke(std::forward<F>(funct), this->span);
            };

            template<class F> auto exclusive(F&& funct)
            {
                std::unique_lock lock(*this->mutex);
                return std::invoke(std::forward<F>(funct), this->span);
            };

            // Copies a range of source elements to the underlying span.
            // Returns the byte count that could not be copied.
            template<std::ranges::contiguous_range R> auto copy(const R& src)
                -> std::size_t
            {   // Lock exclusive access to write the range to the memory span.
                return this->exclusive([&](auto& span)
                {   // Get the amount of bytes to copy.
                    auto dst_bytes     = std::ranges::size(span) * sizeof(T);
                    auto src_bytes     = std::ranges::size(src) * sizeof(std::ranges::range_value_t<R>);
                    auto bytes_to_copy = std::min(dst_bytes, src_bytes);

                    // Copy the correct amount of bytes
                    std::memcpy(std::ranges::data(span), std::ranges::data(src), bytes_to_copy);

                    return bytes_to_copy - src_bytes;
                });
            };

            // Clones the underlying std::span to an input range.
            template<std::ranges::contiguous_range R> auto clone(R& dest) const
                -> std::size_t
            {   // Lock shared access to clone the span to the range.
                return this->shared([&](const auto& span)
                {   // Get the amount of bytes to copy.
                    auto src_bytes     = std::ranges::size(span) * sizeof(T);
                    auto dst_bytes     = std::ranges::size(dest) * sizeof(std::ranges::range_value_t<R>);
                    auto bytes_to_copy = std::min(dst_bytes, src_bytes);

                    // Copy the correct amount of bytes
                    std::memcpy(std::ranges::data(dest), std::ranges::data(span), bytes_to_copy);

                    return bytes_to_copy - src_bytes;
                });
            };
        };

        export constexpr inline std::size_t default_base_addr  = 0x2000;
        export constexpr inline std::size_t default_page_size  = 0x1000;
    };

    export struct Memory
    {
        using VMSpan    = std::pair<std::uintptr_t, std::size_t>;
        using PageVec   = std::vector<mem::Page>;
        using BytesVec  = std::vector<std::byte>;
        using FreeVec   = std::vector<VMSpan>;

        // Next free address.
        std::atomic_uintptr_t next_addr;
        std::uintptr_t        base_addr;

        // Page mutexes and flags.
        PageVec               pages;

        // Freed memory spans.
        FreeVec               freed;

        // Raw physical bytes.
        BytesVec              bytes;

        // Size of a memory page.
        std::size_t           page_size;

        // Locks during resizing operations.
        std::mutex            mutex;

        enum class Err: std::uint8_t
        {
            OutOfRange,
            NoPermission,
        };

        template<class T> using Result = xxas::Result<T, Err>;

        constexpr Memory(const std::uintptr_t base_addr = mem::default_base_addr, const std::size_t page_sz = mem::default_page_size)
            : next_addr(base_addr), page_size(page_sz), base_addr(base_addr)
        {
            bytes.reserve(1024 * 1024);
        };

        // Aligned allocation of a page with flags
        constexpr auto allocate(const std::size_t size, const mem::Flags flags = mem::Flags::Default, const std::size_t alignment = alignof(std::max_align_t))
            -> Result<std::uintptr_t>
        {
            std::uintptr_t addr = this->next_addr.fetch_add(size + alignment);
            std::uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
            std::size_t offset     = aligned - 0x1000; // base address assumed

            if (this->bytes.size() < offset + size)
            {   // Lock the resizing mutex.
                std::scoped_lock lock(this->mutex);

                // Resize to make space for the allocation.
                this->bytes.resize(offset + size);
            };

            this->pages.push_back(mem::Page
            {
                aligned,
                size,
                flags,
            });

            return aligned;
        };

        // Deallocate memory by virtual address
        constexpr void free(const std::uintptr_t vaddr)
        {
            std::scoped_lock lock(this->mutex);

            auto page_it = std::ranges::find_if(this->pages, [&](auto& page)
            {
                return page.vaddr == vaddr;
            });

            if(page_it == this->pages.end())
            {
                return;
            };

            auto vm_span = VMSpan(page_it->vaddr, page_it->size);
            this->pages.erase(page_it);

            auto insert_pos = std::ranges::lower_bound(this->freed, vm_span.first, {}, &VMSpan::first);

            if(insert_pos != this->freed.begin())
            {
                auto prev = std::prev(insert_pos);
                if(prev->first + prev->second == vm_span.first)
                {
                    vm_span.first = prev->first;
                    vm_span.second += prev->second;
                    this->freed.erase(prev);
                };
            };

            if(insert_pos != this->freed.end() && vm_span.first + vm_span.second == insert_pos->first)
            {
                vm_span.second += insert_pos->second;
                insert_pos = this->freed.erase(insert_pos);
            };

            this->freed.insert(insert_pos, vm_span);

            std::uintptr_t max_vaddr = 0;
            for(const auto& page: this->pages)
            {
                max_vaddr = std::max(max_vaddr, page.vaddr + page.size);
            };

            for(auto it = this->freed.rbegin(); it != this->freed.rend(); ++it)
            {
                std::uintptr_t span_end = it->first + it->second;
                if(span_end == this->next_addr.load())
                {
                    this->next_addr.store(it->first);
                    this->bytes.resize(it->first);
                    this->freed.erase(std::next(it).base());
                }
                else break;
            };
        };

        // Get a thread-safe shared memory slice.
        template<class T = std::byte> constexpr auto slice(const std::uintptr_t vaddr, const std::size_t vsize)
            -> Result<mem::Shared<T>>
        {
            for(auto& page: this->pages)
            {
                if (page.contains(vaddr))
                {
                    std::size_t offset = vaddr - page.vaddr;
                    return mem::Shared<T>
                    {
                        .span = std::span<T>
                        {
                            reinterpret_cast<T*>(bytes.data() + (page.vaddr - 0x1000) + offset),
                            vsize / sizeof(T)
                        },
                        .mutex = page.mutex
                    };
                };
            };

            return xxas::error(Err::OutOfRange, std::format("vaddr of {:#x} is out of range", vaddr));
        };

      protected:
        constexpr auto peek(std::uintptr_t vaddr)
            -> Result<std::uintptr_t>
        {
            for(auto& page: this->pages)
            {
                if (page.contains(vaddr))
                {
                    std::size_t offset   = vaddr - page.vaddr;
                    std::size_t absolute = (page.vaddr - this->base_addr) + offset;

                    if(absolute < this->bytes.size())
                    {
                        return *reinterpret_cast<std::uintptr_t*>(this->bytes.data() + absolute);
                    };
                };
            };

            return xxas::error(Err::OutOfRange, std::format("vaddr of {:#x} is out of range", vaddr));
        }
    };
};
