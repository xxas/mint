import std;
import xxas;
import mint;


namespace mint_tests
{
    using namespace mint;

    // Allocate, write and read capabilities.
    constexpr auto awr()
    {
        mint::Memory memory{};

        // Allocate 256 bytes with read and write.
        auto alloc_result = memory.alloc(0x100);
        xxas::assert(alloc_result.has_value(), "alloc_result.has_value()");

        // Create some arbitrary data (same size in bytes as allocation, 256 bytes or 64 u32s).
        std::array<std::uint32_t, 64> data{};
        std::ranges::iota(data.begin(), data.end(), 0x200);

        // assert the ability to write.
        xxas::assert(memory.write(alloc_result.value(), data).has_value(), "memory.write(...).has_value()");

        // Read the first integer.
        auto read_result = memory.read(alloc_result.value(), sizeof(std::uint32_t));

        // Ensure the wrote value in memory is 0x200.
        xxas::assert_eq(*reinterpret_cast<std::uint32_t*>(read_result->data()), 0x200);
    };

    // Reading and writing cross threads.
    constexpr auto wr_ct()
    {
        mint::Memory memory{};

        // Allocate 256 bytes with read and write.
        auto alloc_result = memory.alloc(0x100);
        xxas::assert(alloc_result.has_value(), "alloc_result.has_value()");

        // Get the virtual address of the allocation.
        auto vaddr = alloc_result.value();

        // Create some more arbitrary data (again, match size of allocation).
        std::array<std::uint32_t, 64> data{};
        std::ranges::iota(data.begin(), data.end(), 0xA5);

        // Thread for writing to memory.
        std::thread writer([&memory, vaddr, &data]
        {
            xxas::assert(memory.write(vaddr, data).has_value(), "memory.write(...).has_value()");
        });

        writer.join();

        std::thread reader([&memory, vaddr, &data]
        {   // Read the first integer.
            auto read_result = memory.read(vaddr, sizeof(std::uint32_t));

            // Cast to a std::uint32_t.
            auto value = *reinterpret_cast<const std::uint32_t*>(read_result->data());
            xxas::assert_eq(value, 0xA5);
        });

        reader.join();
    };

    constexpr xxas::Tests memory
    {
       awr, wr_ct,
    };
};

int main()
{
    return mint_tests::memory();
};
