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
{   // Process environment block.
    export struct Peb
    {   // References to the CPU and memory.
        using CpuPtr        = std::shared_ptr<Cpu>;
        using MemPtr        = std::shared_ptr<Memory>;
        using ThreadFilePtr = std::shared_ptr<ThreadFile>;

        CpuPtr cpu_ptr;
        MemPtr mem_ptr;

        template<Arch A> auto thread_file(const std::size_t& thread_id)
            -> ThreadFilePtr
        {   // Return a std::reference_wrapper of the ThreadFile.
            return this->cpu_ptr->try_init<A>(thread_id);
        };
    };
 
    // Thread environment block.
    export struct Teb
    {
        using PebPtr        = std::shared_ptr<Peb>;
        using ThreadFilePtr = std::shared_ptr<ThreadFile>;

        // Current thread id.
        std::size_t thread_id;
 
        // Reference to process environment block.
        PebPtr peb_ptr;

        // Stack frame for the current thread.
        StackFrame stack_frame;

        template<Arch A> auto thread_file()
            -> ThreadFilePtr
        {   // Get the thread file for the current thread id.
            return this->peb_ptr->thread_file<A>(this->thread_id);
        };
    };
};
