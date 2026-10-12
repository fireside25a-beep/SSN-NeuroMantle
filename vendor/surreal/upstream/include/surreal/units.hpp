#pragma once
#include "surreal/exact.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace surreal {
struct Dimension {
  std::int8_t length = 0;
  std::int8_t mass = 0;
  std::int8_t time = 0;
  std::int8_t charge = 0;
  std::int8_t temperature = 0;
  friend bool operator==(const Dimension&, const Dimension&) = default;
  std::string str() const;
};
Dimension operator+(Dimension a, Dimension b);
Dimension operator-(Dimension a, Dimension b);
Dimension operator*(Dimension a, int n);

struct UnitDef {
  std::string name;
  Dimension dimension;
  Rational scale_to_si;
};
const UnitDef& unit(std::string_view name);

struct Quantity {
  Rational si_value;
  Dimension dimension;
  static Quantity from(Rational value, std::string_view unit_name);
  static Quantity si(Rational value, Dimension dimension);
  Rational value_in(std::string_view unit_name) const;
  std::string canonical() const;
};
Quantity operator+(const Quantity&, const Quantity&);
Quantity operator-(const Quantity&, const Quantity&);
Quantity operator*(const Quantity&, const Quantity&);
Quantity operator/(const Quantity&, const Quantity&);
Quantity scale(const Quantity&, const Rational&);

struct QuantityInterval {
  Interval si_value;
  Dimension dimension;
  std::string canonical() const;
};

inline constexpr Dimension Dimless{};
inline constexpr Dimension Length{1,0,0,0,0};
inline constexpr Dimension Mass{0,1,0,0,0};
inline constexpr Dimension Time{0,0,1,0,0};
inline constexpr Dimension Charge{0,0,0,1,0};
inline constexpr Dimension Temperature{0,0,0,0,1};
inline constexpr Dimension Speed{1,0,-1,0,0};
inline constexpr Dimension Acceleration{1,0,-2,0,0};
inline constexpr Dimension Area{2,0,0,0,0};
inline constexpr Dimension Force{1,1,-2,0,0};
inline constexpr Dimension Energy{2,1,-2,0,0};
inline constexpr Dimension Pressure{-1,1,-2,0,0};
inline constexpr Dimension Density{-3,1,0,0,0};
inline constexpr Dimension Frequency{0,0,-1,0,0};
inline constexpr Dimension ElectricPotential{2,1,-2,-1,0};
inline constexpr Dimension EntropyDim{2,1,-2,0,-1};
inline constexpr Dimension ElectricField{1,1,-2,-1,0};
inline constexpr Dimension MagneticField{0,1,-1,-1,0};
inline constexpr Dimension Permeability{1,1,0,-2,0};
inline constexpr Dimension Permittivity{-3,-1,2,2,0};

struct FormulaResult {
  QuantityInterval value;
  std::string formula_id;
};
FormulaResult evaluate_formula(std::string_view formula_id, const std::vector<Quantity>& args,
                               unsigned sqrt_digits = 36);
std::vector<std::string> formula_catalogue_json_lines();
}  // namespace surreal
