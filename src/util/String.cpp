#include "String.h"

#include <unicode/unistr.h>

namespace util::string {

std::string from_char16(const char16_t* str) {
    icu::UnicodeString unicode(str);
    std::string        result;
    return unicode.toUTF8String(result);
}

bool to_char16(const std::string& str, char16_t* des, size_t len) {
    // icu::UnicodeString uses UTF-16 to store strings by default.

    auto unicode = icu::UnicodeString::fromUTF8(str);
    auto length  = unicode.length();
    if ((size_t)length > len - 1) return false;
    unicode.extract(0, length, reinterpret_cast<UChar*>(des));
    return true;
}

std::optional<uint32_t> to_uint32(const std::string& str) {
    char* str_end = nullptr;
    errno         = 0;

    unsigned long value = std::strtoul(str.c_str(), &str_end, 0);

    if (str_end == str.c_str()) return {};
    if (errno == ERANGE) return {};
    if (*str_end != '\0') return {};

    return value;
}

void remove_prefix(std::string& str, const std::string& prefix) {
    if (str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0) {
        str.erase(0, prefix.size());
    }
}

void remove_suffix(std::string& str, const std::string& suffix) {
    if (str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0) {
        str.erase(str.size() - suffix.size());
    }
}

} // namespace util::string