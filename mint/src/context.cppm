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
        using CpuRef = std::reference_wrapper<Cpu>;
        using MemRef = std::reference_wrapper<Memory>;
 
        CpuRef cpu_ref;
        MemRef mem_ref;

        using ThreadFileRef = std::reference_wrapper<arch::ThreadFile>;

        auto cpu()
            -> Cpu&
        {
            return this->cpu_ref.get();
        };

        auto mem()
            -> Memory&
        {
            return this->mem_ref.get();
        };

        template<const arch::RegInit Init> auto thread_file(const std::size_t& thread_id)
            -> ThreadFileRef
        {   // Return a std::reference_wrapper of the ThreadFile.
            return std::ref(this->cpu_ref.get().try_init<Init>(thread_id));
        };
    };
 
    // Thread environment block.
    export struct Teb
    {
        using PebRef        = std::reference_wrapper<Peb>;
        using CpuRef        = std::reference_wrapper<Cpu>;
        using MemRef        = std::reference_wrapper<Memory>;
        using ThreadFileRef = Peb::ThreadFileRef;

        // Current thread id.
        std::size_t thread_id;
 
        // Reference to process environment block.
        PebRef peb_ref;

        // Stack frame for the current thread.
        StackFrame stack_frame;

        auto peb()
            -> Peb&
        {
            return this->peb_ref.get();
        };

        template<const arch::RegInit Init> auto thread_file()
            -> ThreadFileRef
        {   // Get the thread file for the current thread id.
            return this->peb().thread_file<Init>(this->thread_id);
        };

        auto stackframe()
            -> StackFrame&
        {
            return this->stack_frame;
        };
    };
};
