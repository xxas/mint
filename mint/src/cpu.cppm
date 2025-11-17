export module mint: cpu;

import std;
import xxas;
import :memory;
import :stackframe;
import :traits;
import :arch;

/*** **
 **
 **  module:   mint: cpu
 **  purpose:  Multi-threaded capable virtual CPU management.
 **
 *** **/

namespace mint
{
    export struct ThreadData
    {   // Current instruction being executed.
        std::size_t ip;

        // Raw underlying bytes for each register file.
        Registers registers;

        // Thread stackframe.
        StackFrame stackframe;
    };

    export template<const auto& arch> struct Thread
    {
        using Inner    = std::thread;
        using ThreadId = std::thread::id;

        // Inner std::thread data.
        std::thread inner;

        // Thread registers, stack frame.
        ThreadData  data;

        // Default initialization of thread data.
        constexpr static auto from(auto&& funct)
            -> Thread<arch>
        {
            auto data = ThreadData
            {
                .ip        = 0,
                .registers = arch.get_registers(),
            };

            return Thread
            {
                .inner = std::thread(std::move(funct)),
                .data  = data,
            };
        };

        auto get_id() const noexcept
            -> std::optional<std::thread::id>
        {
            return this->inner.get_id();
        };

        auto operator==(const Thread<arch>& other) const noexcept
            -> bool
        {
            return this->get_id() == other.get_id();
        }
    };

    export template<const auto& arch> struct Cpu
    {
        using ThreadVec = std::vector<Thread<arch>>;
        ThreadVec threads;

        // Initializes a new thread and returns the index.
        auto new_thread(auto&& funct)
            -> std::size_t
        {
            this->threads.push_back(Thread<arch>::from(std::move(funct)));

            // Return the thread id.
            return this->threads.back().get_id();
        };

        // Return an iterator to the thread with the given id.
        auto find_thread(const std::size_t id)
            -> ThreadVec::iterator
        {
            return std::ranges::find_if(this->threads, [id](const auto& thread)
            {
                  return thread.get_id() == id;
            });
        };
    };
};
