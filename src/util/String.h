#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace util::string {

std::string from_char16(const char16_t* str);
bool        to_char16(const std::string& str, char16_t* des, size_t len);

// TODO: Replace with: std::expected
std::optional<uint32_t> to_uint32(const std::string& str);

void remove_suffix(std::string& str, const std::string& suffix);


} // namespace util::string