import std;
import xxas;
import mint;

namespace mint_tests
{
    using namespace mint;

    constexpr static auto keywords = arch::Keywords
    {   // Registers.
        std::pair{"gp0", Traits{traits::Bitness::b64, traits::Source::Register}},
        std::pair{"gp1", Traits{traits::Bitness::b64, traits::Source::Register}},
        std::pair{"gp2", Traits{traits::Bitness::b64, traits::Source::Register}},

        // Misc keywords.
        std::pair{"dword", Traits{traits::Bitness::b64}},
        std::pair{"word",  Traits{traits::Bitness::b32}},
        std::pair{"ptr",   Traits{traits::Source::Memory}},
    };

    constexpr static auto insns = arch::Insns
    {
        std::pair{"println", [](const auto& src) -> void { 
            std::println("src: type: {}, value: {}", typeid(decltype(src)).name(), src); 
        }},
        std::pair{"mov", [](auto& dest, const auto& src) -> void { 
            dest = src; 
        }},
        std::pair{"add", [](auto& dest, const auto& a, const auto& b) -> void {
            dest = a + b;
        }},
    };

    constexpr inline Arch arch
    {
        insns, keywords
    };

    void jit_instance_creation()
    {
        // Test basic instance creation and memory allocation.
        auto instance = InstanceBuilder<arch>()
            .memory_layout(MemoryDescriptor{})
            .build();

        // Allocate stack memory.
        auto stack_alloc = instance.inner.mem->allocate(stack::default_size);
        xxas::assert(stack_alloc.has_value(), "Stack allocation should succeed");

        std::println("Stack allocated at: {:#x}", *stack_alloc);

        // Allocate some data memory.
        auto data_alloc = instance.inner.mem->allocate(0x1000);
        xxas::assert(data_alloc.has_value(), "Data allocation should succeed");

        std::println("Data allocated at: {:#x}", *data_alloc);

        // Verify memory is writable.
        auto slice = instance.inner.mem->slice<std::uint64_t>(*data_alloc, sizeof(std::uint64_t));
        xxas::assert(slice.has_value(), "Memory slice should succeed");

        std::uint64_t test_val = 0xDEADBEEF;
        auto copy_result = slice->copy(std::span<const std::uint64_t>(&test_val, 1));
        xxas::assert(copy_result == 0u, "Copy should succeed");

        // Read back and verify.
        auto read_val = slice->shared([](const auto& span) { return span[0]; });
        xxas::assert_eq(read_val, test_val);

        std::println("Memory write/read test passed!");
    }

    void jit_thread_context_creation()
    {
        // Test thread context initialization.
        auto instance = InstanceBuilder<arch>()
            .memory_layout(MemoryDescriptor{})
            .build();

        auto stack_alloc = instance.inner.mem->allocate(stack::default_size);
        xxas::assert(stack_alloc.has_value(), "Stack allocation failed");

        // Create process context.
        auto process_ptr = std::make_shared<ProcessContext<arch>>(
            instance.inner.cpu,
            instance.inner.mem
        );
    };


    constexpr xxas::Tests jit
    {
        jit_instance_creation,
        jit_thread_context_creation,
    };
};

int main()
{
    return mint_tests::jit();
}
