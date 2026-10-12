#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern "C" int surreal_getrandom_u64(std::uint64_t* out);
extern "C" std::uint64_t surreal_rot_xor64(std::uint64_t x);
extern "C" std::uint64_t surreal_unrot_xor64(std::uint64_t x);

namespace surreal {
inline constexpr std::size_t kMaxEntropyAuditDraws = 1'000'000;
std::array<std::uint32_t,4> philox4x32_10(std::array<std::uint32_t,4> counter,
                                         std::array<std::uint32_t,2> key);
class EntropyLedger {
 public:
  explicit EntropyLedger(std::optional<std::uint64_t> seed = std::nullopt, std::uint64_t stream_id = 0);
  std::uint64_t draw();
  EntropyLedger fork_independent(std::string_view domain) const;
  const std::string& root() const noexcept { return root_; }
  const std::string& initial_root() const noexcept { return initial_root_; }
  const std::vector<std::uint64_t>& draws() const noexcept { return draws_; }
  bool replayable() const noexcept { return seed_.has_value(); }
  std::uint64_t stream_id() const noexcept { return stream_id_; }
  std::uint64_t counter() const noexcept { return counter_; }
  std::string audit_log() const;
 private:
  std::optional<std::uint64_t> seed_;
  std::uint64_t stream_id_ = 0;
  std::uint64_t counter_ = 0;
  std::string initial_root_;
  std::string root_;
  std::vector<std::uint64_t> draws_;
};
bool verify_entropy_log(std::string_view log, std::string_view expected_root, std::string* error = nullptr);
}  // namespace surreal
