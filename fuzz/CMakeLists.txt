# Built with -DTHEOS_FUZZ=ON using Clang 19+; run e.g. ./build/fuzz/decode_fuzzer -max_total_time=60 fuzz/corpus/decode
foreach(target decode_fuzzer parse_fuzzer)
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE theos_core theos_warnings)
    target_link_options(${target} PRIVATE -fsanitize=fuzzer)
endforeach()
