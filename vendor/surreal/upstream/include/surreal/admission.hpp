#pragma once
#include <map>
#include <string>
#include <string_view>

namespace surreal {
inline constexpr std::size_t kMaxAdmissionBytes = 1U << 20;
inline constexpr std::size_t kMaxAdmissionFields = 4096;

struct AdmissionRequest {
  std::map<std::string,std::string,std::less<>> fields;
  std::string canonical() const;
  std::string root() const;
  std::string request_id() const;
  std::string operation() const;
};

struct AdmissionReceipt {
  std::string status;
  std::string request_id;
  std::string operation;
  std::string request_root;
  std::string result_root;
  std::string request_hex;
  std::string result_hex;
  std::string reason;
  std::string receipt_root;
  std::string canonical() const;
};

AdmissionRequest parse_admission_request(std::string_view text);
AdmissionReceipt parse_admission_receipt(std::string_view text);
AdmissionReceipt admit(const AdmissionRequest& request);
bool verify_admission_receipt(const AdmissionReceipt& receipt, std::string* error = nullptr);
std::string admission_description_json();
}  // namespace surreal
