export module mint: linker;

import std;
import xxas;

import :ir;
import :arch;
import :expression;

/*** **
 **
 **  module:   mint: linker
 **  purpose:  Aggregates symbol tables across multiple compiled objects
 **            and resolves cross-object references.
 **
 *** **/

namespace mint
{
    namespace link
    {
        export using SymbolMap = std::unordered_map<std::string_view, std::uint64_t>;

        // Symbol resolution across objects.
        export struct SymbolTable
        {
            SymbolMap symbols;

            // Resolve a symbol name to its address.
            constexpr auto resolve(std::string_view name) const
                -> std::optional<std::uint64_t>
            {
                if(auto it = this->symbols.find(name); it != this->symbols.end())
                {
                    return it->second;
                };
                return std::nullopt;
            };

            // Register a symbol with its address.
            constexpr auto define(std::string_view name, std::uint64_t address)
                -> bool
            {
                auto [_, inserted] = this->symbols.emplace(name, address);
                return inserted;
            };

            // Merge another table's symbols into this one.
            // Returns false if any symbol conflicts (duplicate definitions).
            auto merge(const SymbolTable& other)
                -> bool
            {
                for(const auto& [name, address]: other.symbols)
                {
                    if(!this->define(name, address))
                    {
                        return false;
                    };
                };
                return true;
            };
        };
    };

    // Aggregates symbols across compiled objects.
    export template<const auto& arch> struct Linker
    {
        link::SymbolTable globals;

        // Import symbols from an IR program (labels and data directives).
        // Uses the encoder to estimate instruction sizes for accurate layout.
        template<class Enc>
        auto import_symbols(const ir::Program& program, Enc& encoder)
            -> void
        {
            std::uint64_t current_addr = 0;

            for(const auto& node: program.nodes)
            {
                std::visit(xxas::meta::Overloads
                {
                    [&](const ir::Label& label)
                    {
                        this->globals.define(label.name, current_addr);
                    },
                    [&](const ir::Instruction& insn)
                    {
                        current_addr += encoder.estimate_size(insn);
                    },
                    [&](const ir::Data& data)
                    {
                        this->globals.define(data.label, current_addr);
                        const auto byte_size = Traits{ data.size }.size();
                        current_addr += byte_size * data.values.size();
                    },
                }, node);
            };
        };

        // Resolve a symbol across all imported objects.
        auto resolve(std::string_view name) const
            -> std::optional<std::uint64_t>
        {
            return this->globals.resolve(name);
        };
    };
};
