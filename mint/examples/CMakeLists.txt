cmake_minimum_required(VERSION 3.29.0 FATAL_ERROR)

# Enable experimental C++ STD import and modules.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
set(CMAKE_CXX_MODULE_STD ON)

# Set the C++ standard.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(add_mint_example name)
    # Create the example executable.
    add_executable(example_${name} ${name}.cpp)

    # Link the libraries to the executable.
    target_link_libraries(example_${name} PRIVATE mint xxas)
endfunction()

add_mint_example(simple)
