cmake_minimum_required(VERSION 3.20)
project(TheosEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statically link MinGW runtime to prevent any missing DLL / startup issues
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")

# Include header search path
include_directories(include)

# Build the core theological engine library
add_library(theos_core STATIC
    src/core/parser.cpp
    src/core/validator.cpp
    src/core/exporter.cpp
    src/core/format.cpp
)

# Build the CLI executable
add_executable(theosc
    src/cli/main.cpp
)

# Link the core library to the CLI
target_link_libraries(theosc PRIVATE theos_core)