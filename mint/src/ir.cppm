export module mint: ir;

import std;
import xxas;

import :operand;
import :expression;
import :traits;

/***
 **  module:   mint: ir
 **  purpose:  Intermediate representation for parsed assembly with
 **            instructions, labels, and data directives.
 ***/

namespace mint
{
    namespace ir
    {
        // IR instruction representation.
        export struct Instruction
        {
            std::string_view     name;
            std::vector<Operand> operands;
            std::size_t          line;
        };

        // IR label.
        export struct Label
        {
            std::string_view name;
            std::size_t      line;
        };

        // IR data directive.
        export struct Data
        {
            std::string_view        label;
            traits::Bitness         size;
            std::vector<Expression> values;
            std::size_t             line;
        };

        // IR node types.
        export struct Node: std::variant<Instruction, Label, Data>
        {
            using std::variant<Instruction, Label, Data>::variant;
        };

        // IR program.
        export struct Program
        {
            std::vector<Node> nodes;
            std::string_view  entry_point{ "main" };

            constexpr auto add_instruction(Instruction insn)
                -> Program&
            {
                this->nodes.emplace_back(std::move(insn));
                return *this;
            };

            constexpr auto add_label(Label label)
                -> Program&
            {
                this->nodes.emplace_back(std::move(label));
                return *this;
            };

            constexpr auto add_data(Data data)
                -> Program&
            {
                this->nodes.emplace_back(std::move(data));
                return *this;
            };
        };
    };
};
