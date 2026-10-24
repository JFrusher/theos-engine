# CLAUDE.md - Developer Guidelines for Theos Engine

## Build & Test Commands
- **Configure CMake:** `cmake -B build -DCMAKE_BUILD_TYPE=Release`
- **Build Project:** `cmake --build build -j$(nproc)`
- **Run Tests:** `ctest --test-dir build --output-on-failure`
- **Run Binary:** `./build/theosc`

## Coding Standards & C++20 Style Guide
- **Language Standard:** Strict C++23 (`std::expected`, `std::span`, `std::unique_ptr`, `constexpr`, concepts). GCC 13+ / Clang 19+ / MinGW-w64 GCC 13+ (Clang 18 cannot see `std::expected` in libstdc++ 13).
- **Memory Safety:** Avoid raw `new`/`delete`. Use smart pointers and deterministic resource acquisition (RAII).
- **Binary Format:** `.theos` is read and written field by field with explicit little-endian encoding and bounds checks (`src/core/binary.cpp`); never `reinterpret_cast` file bytes into structs. Spec: `docs/spec/theos-v2.md`.
- **Golden Files:** `THEOS_UPDATE_GOLDEN=1 ./build/tests/theos_tests "[golden]"` regenerates `tests/golden/**` outputs; review the diff before committing.
- **Error Handling:** Use `std::expected` or exceptions with explicit error categories for file I/O and syntax parsing failures. Never leave parsing streams unchecked.
- **Const Correctness:** Make member functions `const` wherever state is unmodified.