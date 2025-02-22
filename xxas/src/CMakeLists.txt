cmake_minimum_required(VERSION 3.29.0 FATAL_ERROR)

# Enable experimental C++ STD import and modules.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "0e5b6991-d74f-4b3d-a41c-cf096e0b2508")
set(CMAKE_CXX_MODULE_STD ON)

# Set the C++ standard.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Create the `xxas` library.
add_library(xxas)
target_sources(
    xxas         PUBLIC FILE_SET
    xxas_modules TYPE CXX_MODULES FILES

    # Target modules.
    mod.cppm
    meta.cppm
    format.cppm
    error.cppm
    tests.cppm
    fnv1a.cppm
    multiarray.cppm
    bmultimap.cppm
)

target_link_libraries(xxas PRIVATE m)
