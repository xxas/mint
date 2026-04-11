cmake_minimum_required(VERSION 3.29.0 FATAL_ERROR)

# Enable experimental C++ STD import and modules.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
set(CMAKE_CXX_MODULE_STD ON)

# Set the C++ standard.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Create the `mint` library.
add_library(mint)
target_sources(
    mint         PUBLIC FILE_SET
    mint_modules TYPE CXX_MODULES FILES

    mod.cppm

    memory.cppm
    stackframe.cppm
    cpu.cppm
    context.cppm

    scalar.cppm
    traits.cppm
    semantics.cppm
    arch.cppm

    expression.cppm
    operand.cppm
    instruction.cppm

    binding.cppm
    jit_compiler.cppm
    object.cppm
    instance.cppm

    lexer.cppm

    ir.cppm
    parser.cppm
    lowering.cppm
    linker.cppm

    binary.cppm
    assembler.cppm

    lifter.cppm
    executor.cppm
)



target_link_libraries(mint PRIVATE m)
target_link_libraries(mint PRIVATE xxas)
target_compile_definitions(mint PRIVATE _LIBCPP_ENABLE_EXPERIMENTAL)
