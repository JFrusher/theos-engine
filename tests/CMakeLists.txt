find_package(Catch2 REQUIRED)

add_executable(theos_tests
    test_validator.cpp
)

target_link_libraries(theos_tests PRIVATE theos_core Catch2::Catch2WithMain)

add_test(NAME UnitTests COMMAND theos_tests)