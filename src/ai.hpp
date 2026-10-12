#pragma once
#include <functional>
#include <string>

namespace nm_ai {
using Decision = std::function<std::string(const std::string&)>;
std::string run(const std::string& scenario, const std::string& executable, const Decision& decide, const std::string& socket_path="");
void verify(const std::string& scenario, const std::string& receipt, const Decision& decide, bool live);
}
