cmake_minimum_required(VERSION 3.20)
project(TheosEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings apply to our targets only, never to fetched dependencies
add_library(theos_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(theos_warnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
endif()

# MinGW: link the runtime statically so theosc.exe runs without extra DLLs
if(MINGW)
    add_link_options(-static -static-libstdc++ -static-libgcc)
endif()

# Fuzz builds instrument everything so the fuzzers see coverage inside the library
option(THEOS_FUZZ "Build libFuzzer targets (Clang only)" OFF)
if(THEOS_FUZZ)
    add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(theos_core STATIC
    src/core/analysis.cpp
    src/core/ast.cpp
    src/core/binary.cpp
    src/core/canon.cpp
    src/core/checker.cpp
    src/core/diagnostic.cpp
    src/core/dialectic.cpp
    src/core/driver.cpp
    src/core/exporter.cpp
    src/core/expr.cpp
    src/core/lexer.cpp
    src/core/logic.cpp
    src/core/parser.cpp
    src/core/proof_checker.cpp
    src/core/s5.cpp
    src/core/site.cpp
    src/core/smt.cpp
    src/core/truth_table.cpp
    src/core/types.cpp
    src/core/validator.cpp
)
target_include_directories(theos_core PUBLIC include)
if(EMSCRIPTEN)
    target_compile_definitions(theos_core PRIVATE THEOS_LIBRARY_DIR="/library")
else()
    target_compile_definitions(theos_core PRIVATE THEOS_LIBRARY_DIR="${PROJECT_SOURCE_DIR}/library")
endif()
target_link_libraries(theos_core PRIVATE theos_warnings)

if(EMSCRIPTEN)
    # Browser build: emcmake cmake -B build-wasm -DTHEOS_BUILD_TESTS=OFF && cmake --build build-wasm
    target_compile_options(theos_core PRIVATE -fwasm-exceptions)
    add_executable(theos_wasm src/wasm/theos_wasm.cpp)
    target_compile_options(theos_wasm PRIVATE -fwasm-exceptions)
    target_link_libraries(theos_wasm PRIVATE theos_core theos_warnings embind)
    target_link_options(theos_wasm PRIVATE -fwasm-exceptions -sMODULARIZE=1 -sEXPORT_NAME=createTheos
                        -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1
                        --preload-file ${PROJECT_SOURCE_DIR}/library@/library)
    set_target_properties(theos_wasm PROPERTIES OUTPUT_NAME theos RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/web/dist)
    return()
endif()

add_executable(theosc src/cli/main.cpp)
target_link_libraries(theosc PRIVATE theos_core theos_warnings)
target_compile_definitions(theosc PRIVATE THEOS_VERSION="${PROJECT_VERSION}")

# Release layout: bin/theosc and share/theos/library, which theosc finds relative to itself
install(TARGETS theosc RUNTIME DESTINATION bin)
install(DIRECTORY library/ DESTINATION share/theos/library)
install(FILES LICENSE README.md DESTINATION share/theos)
install(DIRECTORY docs/ DESTINATION share/theos/docs)

if(THEOS_FUZZ)
    add_subdirectory(fuzz)
endif()

option(THEOS_BUILD_TESTS "Build the test suite" ON)
if(THEOS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
