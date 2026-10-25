# CLAUDE.md - Developer Guidelines for Theos Engine

## Build & Test Commands
- **Configure CMake:** `cmake -B build -DCMAKE_BUILD_TYPE=Release`
- **Build Project:** `cmake --build build -j$(nproc)`
- **Run Tests:** `ctest --test-dir build --output-on-failure` (Z3 tests skip if `z3` is not installed)
- **Run Binary:** `./build/theosc` (`check`, `export`, `build`, `inspect`, `analyze`, `prove`, `site`)
- **Check the library:** `for f in library/arguments/*.theo; do ./build/theosc check $f; done`
- **Fuzz (Clang 19):** `cmake -B build-fuzz -DCMAKE_CXX_COMPILER=clang++-19 -DTHEOS_FUZZ=ON -DTHEOS_BUILD_TESTS=OFF`
- **WebAssembly (Emscripten 3.1.74):** `emcmake cmake -B build-wasm -DTHEOS_BUILD_TESTS=OFF && cmake --build build-wasm`, then `node tests/web/wasm_smoke.cjs`
- **README images:** `node tools/readme/render.cjs` regenerates `docs/assets/` from `build/theosc` and `web/dist` (needs ffmpeg and network)

## Layout
- `src/core/`: lexer, parser, validator, types (sorts), checker (decision procedures + `proof_checker`), `s5`, `truth_table`, `logic` (substitution, alpha-equivalence, definitions), `dialectic`, `analysis`, `smt`, `binary`, `exporter`, `site`, `driver` (imports + pipeline).
- `docs/spec/`: the contracts. Change a spec and its implementation in the same PR.
- `library/arguments/`: gallery arguments, all must pass `theosc check`. `library/std/`: importable standard library.

## Coding Standards & C++20 Style Guide
- **Language Standard:** Strict C++23 (`std::expected`, `std::span`, `std::unique_ptr`, `constexpr`, concepts). GCC 13+ / Clang 19+ / MinGW-w64 GCC 13+ (Clang 18 cannot see `std::expected` in libstdc++ 13).
- **Memory Safety:** Avoid raw `new`/`delete`. Use smart pointers and deterministic resource acquisition (RAII).
- **Binary Format:** `.theos` is read and written field by field with explicit little-endian encoding and bounds checks (`src/core/binary.cpp`); never `reinterpret_cast` file bytes into structs. Spec: `docs/spec/theos-v2.md`.
- **Golden Files:** `THEOS_UPDATE_GOLDEN=1 ./build/tests/theos_tests "[golden]"` regenerates `tests/golden/**` outputs; review the diff before committing.
- **Error Handling:** Use `std::expected` or exceptions with explicit error categories for file I/O and syntax parsing failures. Never leave parsing streams unchecked.
- **Const Correctness:** Make member functions `const` wherever state is unmodified.