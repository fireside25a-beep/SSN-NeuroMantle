#pragma once
#include "surreal/entropy.hpp"
#include "surreal/exact.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace surreal {
struct Vec3I {
  Interval x, y, z;
};
struct Body {
  std::string id;
  Rational mass;
  Rational charge;
  Vec3I position;
  Vec3I velocity;
};
struct BodySystem {
  std::vector<Body> bodies;
  Rational radiation_energy{0};
  std::string canonical() const;
};
struct InvariantCertificate {
  bool pass = false;
  std::map<std::string, std::string> claims;
  std::string hash() const;
};
struct LawResult {
  BodySystem state;
  InvariantCertificate invariant;
  std::string detail;
};

struct CosmologyParams {
  Rational omega_r, omega_m, omega_k, omega_l;
};
struct CosmologyState {
  Interval scale_factor;
  std::string world;
};
CosmologyParams cosmology_world(std::string_view world);
CosmologyState friedmann_step(const CosmologyState&, const Rational& dt, unsigned sqrt_digits = 36);
LawResult gravity_kdk(const BodySystem&, const Rational& dt, const Rational& G,
                      const Rational& softening2, unsigned sqrt_digits = 36);
LawResult coulomb_kick(const BodySystem&, const Rational& dt, const Rational& k,
                       const Rational& softening2, unsigned sqrt_digits = 36);
Body photon_propagate(const Body& photon, const Vec3I& direction, const Rational& dt);
LawResult rest_mass_to_radiation(const BodySystem&, std::size_t body_index, const Rational& mass_amount);
LawResult stochastic_two_body_decay(const BodySystem&, std::size_t body_index, Rational probability,
                                    EntropyLedger& entropy);
InvariantCertificate body_invariants(const BodySystem& before, const BodySystem& after);

// Unit-safe reference formulas are declared in surreal/units.hpp.
std::vector<std::string> law_catalogue_json_lines();
}  // namespace surreal
