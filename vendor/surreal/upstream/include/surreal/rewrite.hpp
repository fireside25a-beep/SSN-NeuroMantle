#pragma once
#include "surreal/exact.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace surreal {
inline constexpr std::size_t kMaxLawInstructions = 65'536;
inline constexpr std::size_t kMaxRewriteSteps = 65'536;
inline constexpr std::size_t kMaxRewriteCorpus = 65'536;
enum class OpCode { PushConst, LoadX, Add, Mul, Dup, Swap };
struct Instruction {
  OpCode op;
  Rational value{0};
  std::string canonical() const;
};
struct LawProgram {
  std::vector<Instruction> code;
  Rational evaluate(const Rational& x) const;
  std::string hash() const;
  std::string canonical() const;
};
struct RewriteStep {
  std::string rule;
  std::size_t index = 0;
  std::string before_hash;
  std::string after_hash;
};
struct RewriteCertificate {
  std::string original_hash;
  std::string final_hash;
  std::vector<RewriteStep> steps;
  std::string root() const;
};
struct ForgeResult {
  LawProgram program;
  RewriteCertificate certificate;
  std::vector<std::string> lineage;
};
ForgeResult self_rewrite_kernel(const LawProgram&, const std::vector<Rational>& corpus);
bool verify_rewrite_certificate(const LawProgram& original, const LawProgram& claimed,
                                const RewriteCertificate& certificate);
}  // namespace surreal
