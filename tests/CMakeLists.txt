include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(theos_tests
    test_binary.cpp
    test_canon.cpp
    test_dialectic.cpp
    test_exporter.cpp
    test_golden.cpp
    test_imports.cpp
    test_lexer.cpp
    test_parser.cpp
    test_proofs.cpp
    test_types.cpp
    test_validator.cpp
)
target_link_libraries(theos_tests PRIVATE theos_core theos_warnings Catch2::Catch2WithMain)
target_compile_definitions(theos_tests PRIVATE THEOS_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(theos_tests)
