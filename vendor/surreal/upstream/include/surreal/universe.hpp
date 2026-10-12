#pragma once
#include "surreal/causal.hpp"
#include "surreal/entropy.hpp"
#include "surreal/physics.hpp"
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace surreal {
struct RegionState {
  std::string id;
  std::string kind;
  std::string payload;
  std::string canonical() const;
  std::string hash() const;
};

class Universe {
 public:
  explicit Universe(std::optional<std::uint64_t> entropy_seed = std::nullopt);
  // Insert-only. Silent replacement is forbidden; use replace_region explicitly.
  void put_region(RegionState region);
  void replace_region(RegionState region);
  const RegionState& region(const std::string& id) const;
  const void* region_identity(const std::string& id) const;
  void declare_law(std::string law);
  bool law_declared(std::string_view law) const noexcept;
  // Replay/common-random-number fork: state and seeded entropy position are copied.
  // This intentionally gives both branches identical future draws until their
  // entropy use diverges, useful for paired counterfactual experiments.
  Universe fork() const;
  // Independent stochastic fork: shares immutable state/causal past, but derives
  // a deterministic independent Philox stream from the supplied branch domain.
  Universe fork_independent(std::string_view branch_domain) const;
  void perturb_region(const std::string& id, std::string new_payload);
  std::vector<std::string> invalidated_future(const std::string& event_id) const;
  std::string transition(const std::string& region_id, const std::string& law,
                         const Rational& local_time, const std::vector<std::string>& parents,
                         const std::string& parameters, bool deterministic,
                         const std::function<std::pair<std::string, InvariantCertificate>(const std::string&)>& fn);
  const CausalGraph& causal() const noexcept { return causal_; }
  CausalGraph& causal() noexcept { return causal_; }
  EntropyLedger& entropy() noexcept { return entropy_; }
  const EntropyLedger& entropy() const noexcept { return entropy_; }
  std::string state_root() const;
 private:
  struct MemoValue { std::string after_payload; InvariantCertificate invariant; };
  std::map<std::string, std::shared_ptr<const RegionState>> regions_;
  CausalGraph causal_;
  EntropyLedger entropy_;
  std::map<std::string, MemoValue> memo_;
  std::set<std::string> declared_laws_;
};

struct SparseI64Init { std::size_t index = 0; std::int64_t value = 0; };
struct ReferenceUniverseConfig {
  std::size_t steps = 2;
  std::string world = "lcdm";
  Rational friedmann_dt{BigInt(1),BigInt(100)};
  Rational gravity_dt{BigInt(1),BigInt(100)};
  Rational gravity_g{1};
  Rational softening2{BigInt(1),BigInt(100)};
  Rational decay_probability{BigInt(1),BigInt(16)};
  std::int64_t scalar_threshold = 5;
  int nx = 4, ny = 4, nz = 4;
  BodySystem bodies;
  std::vector<SparseI64Init> ex, ey, ez, bx, by, bz, scalar;
  std::string canonical() const;
  void validate() const;
};
ReferenceUniverseConfig default_reference_config(std::size_t steps, std::string world = "lcdm");
ReferenceUniverseConfig load_reference_config(const std::string& path);

struct RunReport {
  std::string universe_root;
  std::string causal_root;
  std::string entropy_root;
  std::string rewrite_root;
  std::string config_root;
  std::size_t events = 0;
  std::string world;
  std::string canonical() const;
};
RunReport run_universe(const ReferenceUniverseConfig& config, const std::string& out_dir,
                       std::optional<std::uint64_t> seed);
RunReport run_reference_universe(std::size_t steps, const std::string& out_dir,
                                 std::optional<std::uint64_t> seed, const std::string& world);
bool verify_reference_run(const std::string& out_dir, std::string* error = nullptr);
std::string reference_run_manifest_root(const std::string& out_dir);
bool verify_reference_run_anchored(const std::string& out_dir, std::string_view expected_manifest_root,
                                   std::string* error = nullptr);
}  // namespace surreal
