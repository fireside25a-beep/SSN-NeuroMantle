#pragma once
#include <string_view>

namespace surreal {
inline constexpr std::string_view kVersion = "0.5.7.1";
inline constexpr std::string_view kRunReportSchema = "SURREAL_RUN_V2";
inline constexpr std::string_view kReferenceConfigSchema = "SURREAL_REFERENCE_CONFIG_V1";
inline constexpr std::string_view kAdmissionRequestSchema = "SURREAL_ADMISSION_REQUEST_V1";
inline constexpr std::string_view kAdmissionReceiptSchema = "SURREAL_ADMISSION_RECEIPT_V1";
}  // namespace surreal
