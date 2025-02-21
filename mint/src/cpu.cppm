export module mint: cpu;

import std;

import :memory;
import :traits;

/*** **
 **
 **  module:   xxas: cpu
 **  purpose:  Cross-thread safe CPU emulation,
 **            with user-defined register initialization.
 **
 *** **/

namespace mint
{
    namespace cpu
    {
        export using Register  = std::vector<std::byte>;
        export using RegFile   = std::vector<Register>;

        export struct ThreadFile
        {   // Current instruction being executed.
            std::size_t ip;

            // Raw underlying bytes for each register file.
            RegFile reg_file;

            constexpr ThreadFile(RegFile&& reg_file)
              : reg_file{std::move(reg_file)}, ip{} {};
        };

        // User-defined register initialization information.
        export struct RegFileInitializer
        {
            using RegTraits = std::vector<Traits>;

            // Traits of each register.
            RegTraits traits;

            // initialize a register file to the initializers traits.
            auto init() const noexcept
                -> ThreadFile
            {
                RegFile reg_file{};

                for(std::size_t i = 0; i < traits.size(); i++)
                {   // Get the bitness for the register.
                    auto bitness = traits::bitness_for(traits[i].bitness);

                    Register reg{};
                    reg.resize(bitness);

                    // Initialize the register to the user-specified bitness.
                    reg_file[i] = reg;
                };

                return ThreadFile
                {
                    std::move(reg_file),
                };
            };
        };
    };

    export struct Cpu
    {   // Underlying bytes of each register.
        using Register    = cpu::Register;

        // Register files, mapping of register id to the underlying bytes.
        using RegFile     = cpu::RegFile;
        using Initializer = cpu::RegFileInitializer;
        using ThreadFiles = std::unordered_map<std::size_t, cpu::ThreadFile>;

        // Register files for each thread id.
        ThreadFiles   thread_files;

        // User-defined thread file initializer.
        Initializer   initializer;

        auto try_init(const std::size_t& thread_id) noexcept
            -> cpu::ThreadFile&
        {   // Try emplacing a newly constructed thread file if one doesn't already exists for the thread id.
            auto [it, _] = this->thread_files.try_emplace(thread_id, initializer.init());

            // Return the thread file.
            return it->second;
        };
    };
};
