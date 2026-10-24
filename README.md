# Theos Engine (`theosc`)

A C++23 engine and CLI for writing, checking and visualizing formal theological and philosophical arguments.

---

## The Mission

Formal philosophy and historical theology, from scholastic proofs like Aquinas’s *Five Ways* to modern modal logic arguments, rely on rigorous chains of premises, deductions, and dependencies. Yet, these arguments are rarely treated as structured computational artifacts.

Theos bridges this gap. It provides a deterministic pipeline to turn human-readable philosophical arguments into statically checked, deeply validated graph structures, and serializes them into an optimized binary format for high-speed inspection and analysis.

---

## Pipeline

```
.theo source ─► Lexer ─► Parser ─► AST ─► Validator ─► Mermaid exporter
```

Each claim in a `.theo` file carries natural-language text and a formula:

```theo
argument "Modus Ponens Demo"

axiom Obs = source("Field notes", "p. 4")

premise P1 grounded_by [Obs] {
    text:    "If it rains, the street is wet."
    formula: Rain -> Wet
}

premise P2 grounded_by [Obs] {
    text:    "It rains."
    formula: Rain
}

conclusion C1 from [P1, P2] by imp_e {
    text:    "The street is wet."
    formula: Wet
}
```

The full language is specified in [docs/spec/language.md](docs/spec/language.md).

---

## CLI Usage

```sh
theosc check  examples/aquinas_first_way.theo
theosc export examples/resurrection.theo --format mermaid -o resurrection.mmd
```

Diagnostics go to stderr as `file:line:col: error: message` with the source line underneath;
exit code 0 = success, 1 = invalid input, 2 = usage error.

---

## Project Structure

```text
theos-engine/
├── include/theos/   # Public headers
├── src/core/        # Core library (lexer, parser, validator, exporter)
├── src/cli/         # theosc command-line tool
├── docs/spec/       # Language specification
├── examples/        # Sample .theo arguments
└── tests/           # Catch2 unit tests and golden files
```

---

## Building from Source

Requires CMake 3.20+ and a C++23 compiler (GCC 13+, Clang 18+, or MinGW-w64 GCC 13+ via MSYS2).
Tests use Catch2, fetched automatically by CMake.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/theosc
```

On Windows, run the same commands from an MSYS2 UCRT64 shell (add `-G "MinGW Makefiles"` or `-G Ninja` if needed).

See [ROADMAP.md](ROADMAP.md) for where the project is going. Licensed under the [MIT License](LICENSE).
