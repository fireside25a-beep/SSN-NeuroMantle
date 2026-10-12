#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace surreal::detail {
std::string read_file_bounded(const std::filesystem::path& path, std::size_t max_bytes);
void atomic_write_file(const std::filesystem::path& path, std::string_view data);
bool regular_file_nosymlink(const std::filesystem::path& path);
}
