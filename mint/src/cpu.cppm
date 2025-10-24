export module mint: cpu;

import std;
import xxas;
import :memory;
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
    };

    export template<const auto& arch> struct Thread
    {   // Inner std::thread data.
        std::thread inner;

        // Local thread data.
        ThreadData  data;

        // Default initialization of thread data.
        constexpr static auto from(auto&& funct)
        {
            auto data = ThreadData
            {
                .ip        = 0,
                .registers = arch.get_registers(),
            };

            return Thread
            {
                std::thread(std::move(funct)), data,
            };
        };
    };

    export template<const auto& arch> struct Cpu
    {
        using ThreadVec = std::vector<Thread<arch>>;
        ThreadVec threads;

        // Initialize a new thread.
        auto new_thread(auto&& funct)
            -> Thread<arch>&
        {   // Try emplacing a newly constructed thread file if one doesn't already exists for the thread id.
            auto [it, _] = this->threads.emplace_back(Thread<arch>::from(std::move(funct)));

            // Return the thread-file.
            return it;
        };

        // Returns a threads data by id.
        auto get_thread_data(const std::size_t N)
            -> ThreadData
        {
            return this->threads[N].data;
        }
    };
};
