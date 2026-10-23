# CLAUDE.md - Developer Guidelines for Theos Engine

## Build & Test Commands
- **Configure CMake:** `cmake -B build -DCMAKE_BUILD_TYPE=Release`
- **Build Project:** `cmake --build build -j$(nproc)`
- **Run Tests:** `ctest --test-dir build --output-on-failure`
- **Run Binary:** `./build/src/cli/theosc`

## Coding Standards & C++20 Style Guide
- **Language Standard:** Strict C++20 (`std::span`, `std::unique_ptr`, `constexpr`, concepts).
- **Memory Safety:** Avoid raw `new`/`delete`. Use smart pointers and deterministic resource acquisition (RAII).
- **Binary Packing:** All disk structs must use `#pragma pack(push, 1)` to prevent compiler padding misalignment when reading/writing raw binary streams.
- **Error Handling:** Use `std::expected` or exceptions with explicit error categories for file I/O and syntax parsing failures. Never leave parsing streams unchecked.
- **Const Correctness:** Make member functions `const` wherever state is unmodified.