export module mint: object;

import std;
import xxas;

import :lexer;
import :parser;
import :ir;
import :arch;

/*** **
 **
 **  module:   mint: object
 **  purpose:  Compiled translation unit. Wraps an ir::Program parsed from
 **            source code. Library objects have no entry point; executable
 **            objects declare one via the two-argument from() overload.
 **
 *** **/

namespace mint
{
    export template<const auto& arch, const auto& rules> struct Object
    {
        ir::Program program;

        // Parse source into a library object (no entry point).
        static auto from(std::string_view source) -> Object
        {
            auto tokens  = lexer::Lexer<arch, rules>::from(source);
            auto program = Parser<arch>::from(tokens);
            program.entry_point = {};

            return Object{ std::move(program) };
        };

        // Parse source into an executable object with an explicit entry point.
        static auto from(std::string_view source, std::string_view entry) -> Object
        {
            auto tokens  = lexer::Lexer<arch, rules>::from(source);
            auto program = Parser<arch>::from(tokens);
            program.entry_point = entry;

            return Object{ std::move(program) };
        };
    };
};
