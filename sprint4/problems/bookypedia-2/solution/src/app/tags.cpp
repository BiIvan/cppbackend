#include "use_cases.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>
namespace app {
std::vector<std::string> NormalizeTags(const std::string& text) {
    std::vector<std::string> result;
    std::istringstream input{text};
    std::string item;
    while (std::getline(input, item, ',')) {
        std::istringstream words{item};
        std::string word, normalized;
        while (words >> word) {
            if (!normalized.empty()) { normalized += ' '; }
            normalized += word;
        }
        if (!normalized.empty()) { result.push_back(std::move(normalized)); }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    for (const auto& tag : result) {
        std::size_t length = 0;
        for (unsigned char c : tag) { if ((c & 0xc0) != 0x80) { ++length; } }
        if (length > 30) { throw std::invalid_argument("Tag exceeds 30 characters"); }
    }
    return result;
}
}
