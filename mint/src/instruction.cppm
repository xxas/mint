export module mint: instruction;

import std;
import xxas;
import :traits;
import :scalar;
import :semantics;
import :expression;
import :operand;

/*** **
 **
 **  module:   mint: instruction
 **  purpose:  High-level semantically validating instruction containing
 **            an opcode and expression-based operands.
 **
 *** **/

namespace mint
{
    export struct Instruction
    {
        using Operands  = std::vector<Operand>;
        using Opcode    = std::size_t;

        Opcode    opcode;
        Operands  operands;

        // Validate the semantics of the provided operands against the guide slice.
        template<std::size_t N> constexpr auto valid(const std::span<const semantics::Guide, N>& span) const noexcept
            -> bool
        {
            return semantics::valid(span, this->operands);
        };
    };
};
