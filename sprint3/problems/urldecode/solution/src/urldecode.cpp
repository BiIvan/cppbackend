#include "urldecode.h"

#include <stdexcept>

namespace {

int HexDigitValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }

    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }

    return -1;
}

}  // namespace

std::string UrlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    for (std::size_t i = 0; i < str.size(); ++i) {
        const char ch = str[i];

        if (ch == '+') {
            result.push_back(' ');
        } else if (ch == '%') {
            if (str.size() - i < 3) {
                throw std::invalid_argument(
                    "Incomplete percent-encoded sequence"
                );
            }

            const int high = HexDigitValue(str[i + 1]);
            const int low = HexDigitValue(str[i + 2]);

            if (high == -1 || low == -1) {
                throw std::invalid_argument(
                    "Invalid percent-encoded sequence"
                );
            }

            const unsigned char byte =
                static_cast<unsigned char>(high * 16 + low);

            result.push_back(static_cast<char>(byte));
            i += 2;
        } else {
            result.push_back(ch);
        }
    }

    return result;
}