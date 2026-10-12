#pragma once
#include "surreal/universe.hpp"
#include <string>
#include <string_view>

namespace surreal {
struct HorizonRecord {
  RegionState region;
  std::string causal_root;
  std::string commitment;
};
HorizonRecord seal_horizon(const RegionState&, const std::string& causal_root, const std::string& path);
HorizonRecord reopen_horizon(const std::string& path);
HorizonRecord reopen_horizon_anchored(const std::string& path, std::string_view expected_commitment);
}  // namespace surreal
