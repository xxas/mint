export module mint: cpu;

import std;
import xxas;
import :memory;
import :traits;

/*** **
 **
 **  module:   mint: cpu
 **  purpose:  Cross-thread safe CPU emulation,
 **            with user-defined register initialization.
 **
 *** **/

namespace mint
{
    namespace cpu
    {
        export using Register  = std::vector<std::byte>;
        export using RegFile   = std::unordered_map<std::string, Register>;

        export struct ThreadFile
        {   // Current instruction being executed.
            std::size_t ip{};

            // Raw underlying bytes for each register file.
            RegFile reg_file;
        };

        // User-defined register-file initialization information.
        export template<std::size_t N> struct Initializer
        {
            using RegBitness = std::array<std::pair<std::string, std::uint16_t>, N>;
            const RegBitness registers;

            // Initialize a new register file based on the bitness fields.
            auto init() const noexcept
                -> RegFile
            {
                RegFile reg_file{};

                for(std::size_t i = 0; i < this->registers.size(); i++)
                {
                    auto& [name, bitness_index] = this->registers[i];

                    // Get the bitness for the register.
                    auto bitness = traits::bitness_for(bitness_index);

                    Register reg{};
                    reg.resize(bitness);

                    // Initialize the register to the user-specified bitness.
                    reg_file.insert_or_assign(name, std::move(reg));
                };

                return reg_file;
            };
        };
    };

    export struct Cpu
    {   // Underlying bytes of each register.
        using Register    = cpu::Register;
        using RegFile     = cpu::RegFile;
        using ThreadFile  = cpu::ThreadFile;

        // Register-files, mapping of register id to the underlying bytes.
        using ThreadFiles = std::unordered_map<std::size_t, cpu::ThreadFile>;

        // Register files for each thread id.
        ThreadFiles   thread_files;

        template<cpu::Initializer Initializer> auto try_init(const std::size_t& thread_id) noexcept
            -> cpu::ThreadFile&
        {   // Try emplacing a newly constructed thread file if one doesn't already exists for the thread id.
            auto [it, _] = this->thread_files.try_emplace(thread_id,
                ThreadFile
                {
                    .reg_file = std::move(Initializer.init())
                });

            // Return the thread-file.
            return it->second;
        };
    };
};
