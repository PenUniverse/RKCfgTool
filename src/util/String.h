#pragma once

#include <cstdint>
#include <optional>
#include <string>

std::string Char16ToString(const char16_t* str);

bool StringToChar16(const std::string& str, char16_t* des, size_t len);

// TODO: Replace with: std::expected
std::optional<uint32_t> StringToUInt32(const std::string& str);

void StringRemoveSuffix(std::string& str, const std::string& suffix);
