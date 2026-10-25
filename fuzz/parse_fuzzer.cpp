// libFuzzer target: checking arbitrary source text must report diagnostics, never crash
#include "theos/driver.hpp"
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    auto result = Theos::Driver::check_source(std::string(reinterpret_cast<const char*>(data), size), "fuzz.theo");
    (void)Theos::Driver::render_diagnostics(result);
    return 0;
}
