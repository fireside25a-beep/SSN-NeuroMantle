#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace surreal {
inline constexpr std::size_t kMaxQeccQubits = 62;
inline constexpr std::size_t kMaxQeccGenerators = 61;
inline constexpr std::string_view kQeccCertificateMagic = "SQECC001";

struct PauliMask {
  std::uint64_t x = 0;
  std::uint64_t z = 0;
  bool operator==(const PauliMask&) const = default;
};

struct StabilizerCode {
  std::size_t qubits = 0;
  std::vector<PauliMask> generators;
  PauliMask logical_x;
  PauliMask logical_z;
  PauliMask distance_witness;
};

struct QeccProofReport {
  bool valid = false;
  std::size_t qubits = 0;
  std::size_t stabilizer_rank = 0;
  std::size_t logical_qubits = 0;
  std::size_t distance_lower_bound = 0;
  std::size_t distance_upper_bound = 0;
  std::size_t distinct_single_error_syndromes = 0;
  std::string code_root;
  std::string reason;
  std::string canonical() const;
};

bool pauli_commutes(const PauliMask& a, const PauliMask& b) noexcept;
std::size_t pauli_weight(const PauliMask& p) noexcept;
std::uint64_t stabilizer_syndrome(const StabilizerCode& code, const PauliMask& error);
QeccProofReport verify_stabilizer_code(const StabilizerCode& code);
StabilizerCode five_qubit_perfect_code();
std::string qecc_code_root(const StabilizerCode& code);
void write_qecc_certificate(const StabilizerCode& code, const std::string& path);
StabilizerCode parse_qecc_certificate(std::string_view data);
StabilizerCode read_qecc_certificate(const std::string& path);
bool verify_qecc_certificate(const std::string& path, QeccProofReport* report = nullptr,
                             std::string* error = nullptr);
std::string qecc_description_json();
}  // namespace surreal
