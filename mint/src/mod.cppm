export module mint;

// Virtual machine.
export import :memory;
export import :stackframe;
export import :cpu;
export import :context;

// Architecture traits and definitions.
export import :scalar;
export import :traits;
export import :semantics;
export import :arch;

// Backend compilation and execution.
export import :expression;
export import :operand;
export import :instruction;
export import :binding;
export import :jit_compiler;
export import :object;
export import :instance;

// Frontend lexical analysis, language building.
export import :lexer;

// Intermediate representation.
export import :ir;
export import :parser;
export import :lowering;
export import :linker;

// Binary format and assembler.
export import :binary;
export import :assembler;

// Runtime binary lifting and execution.
export import :lifter;
export import :executor;
