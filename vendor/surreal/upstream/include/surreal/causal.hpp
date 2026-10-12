#pragma once
#include "surreal/exact.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace surreal {
inline constexpr std::size_t kMaxCausalEvents = 1'000'000;
inline constexpr std::size_t kMaxEventParents = 65'536;
struct Event {
  std::string id;
  std::vector<std::string> parents;
  std::size_t depth = 0;
  Rational local_time;
  std::string region;
  std::string law;
  std::string before_hash;
  std::string after_hash;
  std::string invariant_hash;
  std::string entropy_root;
  bool memo_reuse = false;
  std::string canonical() const;
};

class CausalGraph {
 public:
  std::size_t validate_admission(const std::vector<std::string>& parents, const Rational& local_time) const;
  std::string append(Event event);
  bool contains(const std::string& id) const;
  const Event& at(const std::string& id) const;
  std::vector<std::string> descendants(const std::string& root) const;
  std::string root_hash() const;
  std::size_t size() const noexcept { return events_.size(); }
  const std::map<std::string, Event>& events() const noexcept { return events_; }
 private:
  std::map<std::string, Event> events_;
};
std::string causal_boundary_root(const CausalGraph&, const std::vector<std::string>& parents);
}  // namespace surreal
