# Mint

**Mint** is a high-level C++26 framework for simulating instruction set architectures. Providing tools for lexical analysis, intermediate language transformation, syntax validation, multi-threaded execution, and memory management.

# Reason

**Mint** serves as an educational and research-focused framework to help developers understand instruction set architectures or prototype custom architectures through a high-level abstraction layer.

# Requirements
**Mint** is suggested to be generated and compiled with the following tooling versions:
```
clang version 20.0.0git (https://github.com/llvm/llvm-project.git 63d088c6e46122bc776f89ded4f285feaab69ae6)
cmake version 3.30.20240726-g26302e1
ninja 1.13.0.git
```

Build commands (replacing *path* with your LLVM path):
```
cmake -G Ninja -B build \
  -DCMAKE_CXX_FLAGS="-stdlib=libc++ -I/path/include/c++/v1 -I/path/include/x86_64-unknown-linux-gnu/c++/v1" \
  -DCMAKE_CXX_COMPILER="clang" \
  -DCMAKE_EXE_LINKER_FLAGS="-L/path/lib/x86_64-unknown-linux-gnu -lc++ -lc++abi" \
  -DCMAKE_INSTALL_RPATH="/path/lib/x86_64-unknown-linux-gnu" \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON

ninja -C build
```
# Testing
**Mint** uses the **xxas** module for creating test cases. Making it compatible with **ctest**:
```
ctest --test-dir build/ --verbose
```
