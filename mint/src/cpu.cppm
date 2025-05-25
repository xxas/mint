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
        using Register      = Register;
        using RegFile       = RegFile;
        using ThreadFilePtr = std::shared_ptr<ThreadFile>;

        // Register-files, mapping of register id to the underlying bytes.
        using ThreadFiles = std::unordered_map<std::size_t, ThreadFilePtr>;

        // Register files for each thread id.
        ThreadFiles   thread_files;

        template<Arch a> auto try_init(const std::size_t thread_id)
            -> ThreadFilePtr
        {   // Try emplacing a newly constructed thread file if one doesn't already exists for the thread id.
            auto [it, _] = this->thread_files.try_emplace(thread_id,
            ThreadFile
            {
                .reg_file = a.reg_file()
            });

            // Return the thread-file.
            return it->second;
        };
    };
};
