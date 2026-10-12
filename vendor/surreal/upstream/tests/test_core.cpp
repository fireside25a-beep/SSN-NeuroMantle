#include "test.hpp"
#include "surreal/entropy.hpp"
#include "surreal/exact.hpp"
#include "surreal/field_kernels.hpp"
#include "surreal/hash.hpp"
#include "surreal/units.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

int main(){using namespace surreal;
  CHECK_EQ(hex(sha256("abc")),std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  Rational a(BigInt(2),BigInt(4));CHECK_EQ(a.str(),std::string("1/2"));CHECK_EQ((a+Rational(1)).str(),std::string("3/2"));CHECK_THROWS(Rational(BigInt(1),BigInt(0)));
  auto s=sqrt_interval(Interval(Rational(2)),50);CHECK(s.lo*s.lo<=Rational(2));CHECK(s.hi*s.hi>=Rational(2));
  Quadratic phi(Rational(1,BigInt(2)),Rational(1,BigInt(2)),BigInt(5));auto rel=phi*phi-phi-Quadratic(Rational(1),Rational(0),BigInt(5));CHECK(rel.a.is_zero()&&rel.b.is_zero());
  CHECK_EQ(SurrealScale::omega()*SurrealScale::infinitesimal(),SurrealScale::constant(Rational(1)));CHECK_EQ(HahnSeries::omega()*HahnSeries::infinitesimal(),HahnSeries::one());
  for(std::uint64_t x:{0ULL,1ULL,0x123456789abcdef0ULL,~0ULL})CHECK_EQ(surreal_unrot_xor64(surreal_rot_xor64(x)),x);
  auto kat=philox4x32_10({0,0,0,0},{0,0});CHECK_EQ(kat[0],0x6627e8d5u);CHECK_EQ(kat[1],0xe169c58du);CHECK_EQ(kat[2],0xbc57ac4cu);CHECK_EQ(kat[3],0x9b00dbd8u);
  EntropyLedger e1(42),e2(42);for(int i=0;i<8;++i)CHECK_EQ(e1.draw(),e2.draw());CHECK_EQ(e1.root(),e2.root());std::uint64_t live=0;CHECK(surreal_getrandom_u64(&live)==0);
  auto ev=Quantity::from(Rational(1),"eV");CHECK_EQ(ev.value_in("eV"),Rational(1));auto furlong=Quantity::from(Rational(1),"furlong"),fortnight=Quantity::from(Rational(1),"fortnight");auto speed=furlong/fortnight;CHECK_EQ(speed.dimension,Speed);CHECK_THROWS(furlong+fortnight);
  CHECK_THROWS((Dimension{127,0,0,0,0}+Length));CHECK_THROWS((Dimension{64,0,0,0,0}*2));
  auto c=Quantity::si(Rational(299792458),Speed);auto hbar=Quantity::si(Rational(BigInt("1054571817"),BigInt("10000000000000000000000000000000000000000000")),Dimension{2,1,-1,0,0});auto G=Quantity::si(Rational(BigInt("667430"),BigInt("10000000000000000")),Dimension{3,-1,-2,0,0});auto pl=evaluate_formula("planck_length",{hbar,G,c});CHECK_EQ(pl.value.dimension,Length);CHECK_THROWS(evaluate_formula("planck_length",{Quantity::from(Rational(1),"kg"),G,c}));
  auto lf=evaluate_formula("lorentz_force_component",{Quantity::from(Rational(2),"C"),Quantity::from(Rational(3),"V_per_m"),Quantity::from(Rational(1),"V_per_m")});CHECK_EQ(lf.value.dimension,Force);CHECK_EQ(lf.value.si_value.lo,Rational(8));
  auto med=evaluate_formula("magnetic_energy_density",{Quantity::from(Rational(2),"T"),Quantity::from(Rational(2),"H_per_m")});CHECK_EQ(med.value.dimension,Pressure);CHECK_EQ(med.value.si_value.lo,Rational(1));
  auto av=evaluate_formula("alfven_speed",{Quantity::from(Rational(2),"T"),Quantity::from(Rational(1),"H_per_m"),Quantity::si(Rational(4),Density)});CHECK_EQ(av.value.dimension,Speed);CHECK(av.value.si_value.contains(Rational(1)));
  auto action=Quantity::si(Rational(2),Dimension{2,1,-1,0,0});auto br=evaluate_formula("bohr_radius",{Quantity::from(Rational(1),"F_per_m"),action,Quantity::from(Rational(2),"kg"),Quantity::from(Rational(1),"C")});CHECK_EQ(br.value.dimension,Length);CHECK_EQ(br.value.si_value.lo,Rational(2));
  auto hge=evaluate_formula("hydrogen_ground_state_energy",{Quantity::from(Rational(2),"kg"),Quantity::from(Rational(1),"C"),Quantity::from(Rational(1),"F_per_m"),Quantity::si(Rational(1),Dimension{2,1,-1,0,0})});CHECK_EQ(hge.value.dimension,Energy);CHECK_EQ(hge.value.si_value.lo,Rational(1));
  auto bhe=evaluate_formula("bekenstein_hawking_entropy",{Quantity::from(Rational(1),"J_per_K"),Quantity::from(Rational(1),"m_per_s"),Quantity::si(Rational(2),Area),Quantity::si(Rational(1),Dimension{3,-1,-2,0,0}),Quantity::si(Rational(1),Dimension{2,1,-1,0,0})});CHECK_EQ(bhe.value.dimension,EntropyDim);CHECK_EQ(bhe.value.si_value.lo,Rational(BigInt(1),BigInt(2)));CHECK(formula_catalogue_json_lines().size()>=25);
  int nx=4,ny=4,nz=4;std::vector<std::int64_t> f(64),lap(64);f[0]=7;surreal_laplacian_i64(f.data(),lap.data(),nx,ny,nz);CHECK(lap[0]==-42);CHECK(scalar_needs_refinement(f,nx,ny,nz,5));
  std::vector<std::int64_t> ex(64),ey(64),ez(64),bx(64),by(64),bz(64);ex[3]=1;ey[11]=-2;auto cert=maxwell_step_certified(ex,ey,ez,bx,by,bz,nx,ny,nz);CHECK(cert.pass);CHECK_EQ(cert.before_hash,cert.after_hash);
  std::vector<std::int64_t> pos{1,2,3},vel{0,0,0},acc{1,-1,2};kick_drift_checked(pos,vel,acc,2);CHECK(pos[0]==5&&pos[1]==-2&&pos[2]==11);
  std::vector<std::int64_t> huge(64,std::numeric_limits<std::int64_t>::max());CHECK_THROWS(scalar_needs_refinement(huge,nx,ny,nz,5));
  return finish_tests();}
