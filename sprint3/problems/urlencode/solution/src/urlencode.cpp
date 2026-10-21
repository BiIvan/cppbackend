#include "urlencode.h"

std::string UrlEncode(std::string_view str) {
    constexpr std::string_view reserved = "!#$&'()*+,/:;=?@[]";
    constexpr char hex[] = "0123456789ABCDEF";

    std::string result;
    result.reserve(str.size());

    for (char ch : str) {
        const auto byte = static_cast<unsigned char>(ch);

        if (byte == ' ') {
            result.push_back('+');
        } else if (byte < 32
                   || byte >= 128
                   || reserved.find(ch) != std::string_view::npos) {
            result.push_back('%');
            result.push_back(hex[byte >> 4]);
            result.push_back(hex[byte & 0x0F]);
        } else {
            result.push_back(ch);
        }
    }

    return result;
}