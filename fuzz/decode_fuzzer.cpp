// libFuzzer target: decoding arbitrary bytes must either succeed or throw FormatError, never crash
#include "theos/binary.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    try {
        auto doc = Theos::Binary::decode(std::as_bytes(std::span(data, size)));
        (void)Theos::AST::dump(doc);
    } catch (const Theos::Binary::FormatError&) {
    }
    return 0;
}
