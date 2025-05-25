import std;
import xxas;
import mint;

#include <experimental/simd>

#if defined(_LIBCPP_EXPERIMENTAL_SIMD) || defined(_LIBCPP_SIMD)
  // Mint uses std::simd
  #ifndef MINT_SIMD
    #define MINT_SIMD 1
  #endif

  // Expose std::experimental::* -> std::*.
  namespace std{ using namespace experimental; };
#endif

namespace mint_tests
{
    using namespace mint;

    // Single‐threaded allocate/write/read.
    constexpr auto awr()
    {
        Memory memory{};

        // Allocate 256 bytes.
        auto alloc_result = memory.allocate(0x100);
        xxas::assert(alloc_result.has_value(), "alloc_result.has_value()");

        // Prepare 64 u32s of data.
        std::array<std::uint32_t, 64> data{};
        std::ranges::iota(data.begin(), data.end(), 0x200);

        // Get a Shared<u32> slice and copy into it.
        auto slice_result = memory.slice<std::uint32_t>(*alloc_result, 0x100);
        xxas::assert(slice_result.has_value(), "slice_result.has_value()");

        // copy returns number of bytes NOT written; expect zero.
        xxas::assert_eq(slice_result->copy(data), 0u);

        // Read back first element.
        auto first = slice_result->shared([&](auto& span)
        {
            return span[0];
        });

        xxas::assert_eq(first, data[0]);
    };


    constexpr auto concurrent_rw()
    {
        Memory memory{};

        // Allocate 4 pages of 256 bytes each.
        auto alloc_result = memory.allocate(0x100 * 4);
        xxas::assert(alloc_result.has_value(), "alloc_result.has_value()");

        // Get shared memory region as u32.
        auto slice_result = memory.slice<std::uint32_t>(*alloc_result, 0x100 * 4);
        xxas::assert(slice_result.has_value(), "slice_result.has_value()");

        constexpr auto page_u32 = 0x100 / sizeof(std::uint32_t);

        auto rng = std::mt19937_64{0x12345};
        auto dist = std::uniform_int_distribution<std::size_t>{0, 3};

        auto writers = std::vector<std::thread>{};

        for(auto i = 0; i < 4; ++i)
        {
            writers.emplace_back([&, i]
            {
                for(auto w = 0; w < 10; ++w)
                {
                    auto page_index = dist(rng);
                    auto sub = slice_result->subrange(page_index * page_u32, (page_index + 1) * page_u32);

                    auto buf = std::array<std::uint32_t, page_u32>{};
                    buf.fill(static_cast<std::uint32_t>((i << 24) | (page_index << 16) | w));

                    xxas::assert_eq(sub.copy(buf), 0u);
                };
            });
        };

        auto readers = std::vector<std::thread>{};

        for(auto j = 0; j < 4; ++j)
        {
            readers.emplace_back([&]
            {
                for(auto r = 0; r < 10; ++r)
                {
                    auto page_index = dist(rng);
                    auto sub = slice_result->subrange(page_index * page_u32, (page_index + 1) * page_u32);
                    auto out = std::array<std::uint32_t, page_u32>{};

                    xxas::assert_eq(sub.clone(out), 0u);

                    for(auto value: out)
                    {
                        xxas::assert_eq(value, out[0]);
                    };
                };
            });
        };

        for(auto& t: writers)
        {
            t.join();
        };

        for(auto& t: readers)
        {
            t.join();
        };
    };

    constexpr auto simd_par()
    {
        Memory memory{};

        // Allocate one page.
        auto alloc_result = memory.allocate(0x100);
        xxas::assert(alloc_result.has_value(), "alloc_result.has_value()");

        auto vaddr = *alloc_result;

        auto slice_result = memory.slice<std::uint32_t>(vaddr, 0x100);
        xxas::assert(slice_result.has_value(), "slice_result.has_value()");

        // SIMD view.
        auto slice_par = slice_result->par<std::uint32_t>();

        // Write SIMD values.
        slice_par.exclusive([&](auto& simd_span)
        {
            for(auto i = 0; i < simd_span.size(); ++i)
            {
                simd_span[i] = std::experimental::native_simd<std::uint32_t>(static_cast<std::uint32_t>(i * 4));
            };
        });

        // Verify SIMD values.
        slice_par.shared([&](const auto& simd_span)
        {
            for(auto i = 0; i < simd_span.size(); ++i)
            {
                xxas::assert_eq(simd_span[i][0], static_cast<std::uint32_t>(i * 4));
            };
        });
    };


    constexpr xxas::Tests memory
    {
        awr, concurrent_rw, simd_par
    };
};

int main()
{
    return mint_tests::memory();
};
