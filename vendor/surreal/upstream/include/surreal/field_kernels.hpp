#pragma once
#include <cstdint>
#include <vector>
#include <string>

extern "C" {
void surreal_laplacian_i64(const std::int64_t* in, std::int64_t* out, int nx, int ny, int nz);
void surreal_divergence_i64(const std::int64_t* x, const std::int64_t* y, const std::int64_t* z,
                            std::int64_t* out, int nx, int ny, int nz);
void surreal_maxwell_leap_i64(std::int64_t* ex, std::int64_t* ey, std::int64_t* ez,
                              std::int64_t* bx, std::int64_t* by, std::int64_t* bz,
                              int nx, int ny, int nz);
void surreal_kick_drift_i64(std::int64_t* pos, std::int64_t* vel, const std::int64_t* acc,
                            std::int64_t dt, int n);
}

namespace surreal {
struct FieldDivergenceCertificate {
  bool pass = false;
  std::string before_hash;
  std::string after_hash;
};
FieldDivergenceCertificate maxwell_step_certified(
    std::vector<std::int64_t>& ex, std::vector<std::int64_t>& ey, std::vector<std::int64_t>& ez,
    std::vector<std::int64_t>& bx, std::vector<std::int64_t>& by, std::vector<std::int64_t>& bz,
    int nx, int ny, int nz);
bool scalar_needs_refinement(const std::vector<std::int64_t>& field, int nx, int ny, int nz,
                             std::int64_t curvature_threshold);
void kick_drift_checked(std::vector<std::int64_t>& pos, std::vector<std::int64_t>& vel,
                        const std::vector<std::int64_t>& acc, std::int64_t dt);
}  // namespace surreal
