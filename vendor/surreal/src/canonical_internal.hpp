#pragma once
#include <string>
#include <string_view>

namespace surreal::detail {
inline void append_field(std::string& out, std::string_view name, std::string_view value) {
  out.append(name);
  out.push_back(':');
  out += std::to_string(value.size());
  out.push_back(':');
  out.append(value);
  out.push_back('\n');
}
}
