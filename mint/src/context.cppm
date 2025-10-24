export module mint: context;

import std;
import xxas;

import :memory;
import :cpu;
import :stackframe;
import :arch;

/*** **
 **
 **  module:   mint: context
 **  purpose:  Process and thread environment context blocks.
 **
 *** **/

namespace mint
{
    export template<const auto& arch> struct ProcessContext
    {
        using CpuPtr    = std::shared_ptr<Cpu<arch>>;
        using MemoryPtr = std::shared_ptr<Memory>;

        CpuPtr    cpu;
        MemoryPtr mem;
    };
 
    export template<const auto& arch> struct ThreadContext
    {
        using ProcessPtr    = std::shared_ptr<ProcessContext<arch>>;

        // Current thread id.
        std::size_t id;
 
        ProcessPtr process;

        // Stack frame for the current thread.
        StackFrame stack_frame;

        // Returns the local thread data for this thread.
        auto get_data()
            -> std::shared_ptr<ThreadData>
        {
            return this->process->cpu->get_thread_data(this->id);
        };
    };
};
