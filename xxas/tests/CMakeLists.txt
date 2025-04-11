cmake_minimum_required(VERSION 3.29.0 FATAL_ERROR)

# Enable experimental C++ STD import and modules.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
set(CMAKE_CXX_MODULE_STD ON)

# Set the C++ standard.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(add_xxas_test name)
    # Create the test executable.
    add_executable(test_${name} ${name}.cpp)

    # Link the library to the executable.
    target_link_libraries(test_${name} PRIVATE xxas)

    # Add the test.
    add_test(NAME xxas_${name} COMMAND test_${name})
endfunction()

# Enable testing.
enable_testing()

# Register tests using the function.
add_xxas_test(bmultimap)
add_xxas_test(fnv1a)
add_xxas_test(bmap)
add_xxas_test(meta)
add_xxas_test(error)
