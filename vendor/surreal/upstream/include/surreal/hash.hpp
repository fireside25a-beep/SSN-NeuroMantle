#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace surreal {
using Digest256 = std::array<std::uint8_t, 32>;
Digest256 sha256(std::span<const std::uint8_t> bytes);
Digest256 sha256(std::string_view text);
std::string hex(const Digest256& d);
Digest256 unhex256(std::string_view s);
std::string hash_join(std::initializer_list<std::string_view> parts);
std::string hex_encode(std::span<const std::uint8_t> bytes);
std::vector<std::uint8_t> hex_decode(std::string_view s);
}  // namespace surreal
