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
    export struct Cpu
    {   // Underlying bytes of each register.
        using Register    = arch::Register;
        using RegFile     = arch::RegFile;
        using ThreadFile  = arch::ThreadFile;

        // Register-files, mapping of register id to the underlying bytes.
        using ThreadFiles = std::unordered_map<std::size_t, arch::ThreadFile>;

        // Register files for each thread id.
        ThreadFiles   thread_files;

        template<arch::RegInit Init> auto try_init(const std::size_t& thread_id) noexcept
            -> arch::ThreadFile&
        {   // Try emplacing a newly constructed thread file if one doesn't already exists for the thread id.
            auto [it, _] = this->thread_files.try_emplace(thread_id,
                ThreadFile
                {
                    .reg_file = std::move(Init())
                });

            // Return the thread-file.
            return it->second;
        };
    };
};
