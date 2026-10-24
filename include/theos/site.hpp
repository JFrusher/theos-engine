#pragma once

#include "theos/driver.hpp"
#include <string>
#include <string_view>
#include <vector>

// Static HTML gallery of checked arguments
namespace Theos::Site {

struct Entry {
    std::string slug;            // file name without extension; the page is <slug>.html
    Driver::Result result;
    std::string source;
};

std::string argument_page(const Entry& entry);
std::string index_page(const std::vector<Entry>& entries);

std::string base64(std::string_view bytes);

} // namespace Theos::Site
