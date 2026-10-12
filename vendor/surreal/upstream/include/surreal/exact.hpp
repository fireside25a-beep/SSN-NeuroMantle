#pragma once
#include <boost/multiprecision/cpp_int.hpp>
#include <compare>
#include <map>
#include <string>
#include <string_view>

namespace surreal {
using BigInt = boost::multiprecision::cpp_int;

class Rational {
 public:
  Rational();
  Rational(long long n);
  Rational(BigInt n, BigInt d);
  static Rational parse(std::string_view s);
  const BigInt& num() const noexcept { return n_; }
  const BigInt& den() const noexcept { return d_; }
  bool is_zero() const noexcept { return n_ == 0; }
  int sign() const noexcept { return n_ == 0 ? 0 : (n_ < 0 ? -1 : 1); }
  std::string str() const;
  Rational abs() const;
  Rational reciprocal() const;
  friend Rational operator+(const Rational&, const Rational&);
  friend Rational operator-(const Rational&, const Rational&);
  friend Rational operator*(const Rational&, const Rational&);
  friend Rational operator/(const Rational&, const Rational&);
  Rational operator-() const;
  friend bool operator==(const Rational&, const Rational&) = default;
  friend std::strong_ordering operator<=>(const Rational&, const Rational&);
 private:
  BigInt n_;
  BigInt d_;
  void normalize();
};

Rational min(const Rational&, const Rational&);
Rational max(const Rational&, const Rational&);
Rational square(const Rational&);

struct Interval {
  Rational lo;
  Rational hi;
  Interval();
  explicit Interval(Rational x);
  Interval(Rational l, Rational h);
  std::string str() const;
  bool contains(const Rational& x) const;
  bool contains_zero() const;
  friend Interval operator+(const Interval&, const Interval&);
  friend Interval operator-(const Interval&, const Interval&);
  friend Interval operator*(const Interval&, const Interval&);
  friend Interval operator/(const Interval&, const Interval&);
  Interval operator-() const;
};
Interval square(const Interval&);
Interval sqrt_interval(const Interval& x, unsigned decimal_digits = 40);

struct Quadratic {
  Rational a;
  Rational b;
  BigInt d;
  Quadratic(Rational a_, Rational b_, BigInt d_);
  std::string str() const;
  friend Quadratic operator+(const Quadratic&, const Quadratic&);
  friend Quadratic operator-(const Quadratic&, const Quadratic&);
  friend Quadratic operator*(const Quadratic&, const Quadratic&);
  Quadratic scale(const Rational&) const;
};

class SurrealScale {
 public:
  // Laurent polynomial in omega with integral exponents and exact rational coefficients.
  SurrealScale();
  static SurrealScale constant(Rational c);
  static SurrealScale omega();
  static SurrealScale infinitesimal();
  void set(long long exponent, Rational coefficient);
  Rational coefficient(long long exponent) const;
  std::string str() const;
  friend SurrealScale operator+(const SurrealScale&, const SurrealScale&);
  friend SurrealScale operator*(const SurrealScale&, const SurrealScale&);
  friend bool operator==(const SurrealScale&, const SurrealScale&) = default;
 private:
  std::map<long long, Rational, std::greater<>> terms_;
  void prune();
};

class HahnSeries {
 public:
  HahnSeries();
  static HahnSeries monomial(Rational exponent, Rational coefficient);
  static HahnSeries one();
  static HahnSeries omega();
  static HahnSeries infinitesimal();
  void set(Rational exponent, Rational coefficient);
  Rational coefficient(const Rational& exponent) const;
  std::string str() const;
  friend HahnSeries operator+(const HahnSeries&, const HahnSeries&);
  friend HahnSeries operator*(const HahnSeries&, const HahnSeries&);
  friend bool operator==(const HahnSeries&, const HahnSeries&) = default;
 private:
  struct Greater {
    bool operator()(const Rational& a, const Rational& b) const { return a > b; }
  };
  std::map<Rational, Rational, Greater> terms_;
  void prune();
};
}  // namespace surreal
