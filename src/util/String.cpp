#include "String.h"

#include <unicode/unistr.h>

std::string Char16ToString(const char16_t* str) {
    icu_75::UnicodeString unicode(str);
    std::string           result;
    return unicode.toUTF8String(result);
}
