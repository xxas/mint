export module mint: instance;

import std;
import xxas;

import :cpu;
import :memory;
import :context;
import :stackframe;
import :semantics;
import :binding;
import :operand;
import :instruction;
import :jit_compiler;

namespace mint
{
    export template<const auto& arch> struct Instance
    {   // Process environment block.
        ProcessContext<arch> inner;
    };

    export template<const auto& arch> struct InstanceBuilder
    {
        MemoryDescriptor mem_desc{};

        // Provide a MemoryDescriptor to tell the Instance where to initialize
        // virtual memory and page widths.
        constexpr auto memory_layout(MemoryDescriptor mem_desc)
            -> InstanceBuilder
        {
            this->mem_desc = mem_desc;
            return *this;
        };

        // Builds a new process instance, including a virtual CPU and Memory handler.
        constexpr auto build()
            -> Instance<arch>
        {
            auto vcpu  = std::make_shared<Cpu<arch>>();
            auto mem  = std::make_shared<Memory>(this->mem_desc.base_addr, this->mem_desc.page_size);

            return Instance
            {
                .inner = ProcessContext(std::move(vcpu), std::move(mem)),
            };
        };
    };
};
