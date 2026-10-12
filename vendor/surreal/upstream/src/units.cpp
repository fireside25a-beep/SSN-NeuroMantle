#include "surreal/units.hpp"
#include <array>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

namespace surreal {
namespace {
void same_dimension(Dimension a, Dimension b, std::string_view what) {
  if (!(a == b)) throw std::invalid_argument(std::string(what) + ": dimension mismatch " + a.str() + " vs " + b.str());
}
void require_dimension(const Quantity& q, Dimension d, std::string_view name) { same_dimension(q.dimension,d,name); }
Rational pi_lo(){return Rational(BigInt(3141592653589793238LL),BigInt(1000000000000000000LL));}
Rational pi_hi(){return Rational(BigInt(3141592653589793239LL),BigInt(1000000000000000000LL));}
QuantityInterval exact_q(Rational v, Dimension d){return {Interval(v),d};}
QuantityInterval interval_q(Interval v, Dimension d){return {std::move(v),d};}
void arity(std::string_view id,const std::vector<Quantity>&a,std::size_t n){if(a.size()!=n)throw std::invalid_argument(std::string(id)+": expected "+std::to_string(n)+" arguments");}
}
std::string Dimension::str() const { return "L^"+std::to_string(length)+" M^"+std::to_string(mass)+" T^"+std::to_string(time)+" Q^"+std::to_string(charge)+" Th^"+std::to_string(temperature); }
namespace {
std::int8_t checked_exp(long long x){if(x<std::numeric_limits<std::int8_t>::min()||x>std::numeric_limits<std::int8_t>::max())throw std::overflow_error("dimension exponent overflow");return static_cast<std::int8_t>(x);}
}
Dimension operator+(Dimension a, Dimension b){return {checked_exp(static_cast<long long>(a.length)+b.length),checked_exp(static_cast<long long>(a.mass)+b.mass),checked_exp(static_cast<long long>(a.time)+b.time),checked_exp(static_cast<long long>(a.charge)+b.charge),checked_exp(static_cast<long long>(a.temperature)+b.temperature)};}
Dimension operator-(Dimension a, Dimension b){return {checked_exp(static_cast<long long>(a.length)-b.length),checked_exp(static_cast<long long>(a.mass)-b.mass),checked_exp(static_cast<long long>(a.time)-b.time),checked_exp(static_cast<long long>(a.charge)-b.charge),checked_exp(static_cast<long long>(a.temperature)-b.temperature)};}
Dimension operator*(Dimension a,int n){return {checked_exp(static_cast<long long>(a.length)*n),checked_exp(static_cast<long long>(a.mass)*n),checked_exp(static_cast<long long>(a.time)*n),checked_exp(static_cast<long long>(a.charge)*n),checked_exp(static_cast<long long>(a.temperature)*n)};}
const UnitDef& unit(std::string_view name){
  static const std::map<std::string,UnitDef,std::less<>> u={
    {"1",{"1",Dimless,Rational(1)}},{"m",{"m",Length,Rational(1)}},{"s",{"s",Time,Rational(1)}},{"kg",{"kg",Mass,Rational(1)}},{"C",{"C",Charge,Rational(1)}},{"K",{"K",Temperature,Rational(1)}},
    {"furlong",{"furlong",Length,Rational(BigInt(201168),BigInt(1000))}},{"fortnight",{"fortnight",Time,Rational(1209600)}},
    {"J",{"J",Energy,Rational(1)}},{"eV",{"eV",Energy,Rational(BigInt(1602176634),BigInt("10000000000000000000000000000"))}},
    {"N",{"N",Force,Rational(1)}},{"Pa",{"Pa",Pressure,Rational(1)}},{"m_per_s",{"m_per_s",Speed,Rational(1)}},{"V_per_m",{"V_per_m",ElectricField,Rational(1)}},{"T",{"T",MagneticField,Rational(1)}},{"H_per_m",{"H_per_m",Permeability,Rational(1)}},{"F_per_m",{"F_per_m",Permittivity,Rational(1)}},{"J_per_K",{"J_per_K",EntropyDim,Rational(1)}}
  }; auto it=u.find(name);if(it==u.end())throw std::invalid_argument("unknown unit: "+std::string(name));return it->second;
}
Quantity Quantity::from(Rational v,std::string_view n){auto&u=unit(n);return {v*u.scale_to_si,u.dimension};}
Quantity Quantity::si(Rational v,Dimension d){return {std::move(v),d};}
Rational Quantity::value_in(std::string_view n)const{auto&u=unit(n);same_dimension(dimension,u.dimension,"unit conversion");return si_value/u.scale_to_si;}
std::string Quantity::canonical()const{return si_value.str()+" SI ["+dimension.str()+"]";}
Quantity operator+(const Quantity&a,const Quantity&b){same_dimension(a.dimension,b.dimension,"addition");return {a.si_value+b.si_value,a.dimension};}
Quantity operator-(const Quantity&a,const Quantity&b){same_dimension(a.dimension,b.dimension,"subtraction");return {a.si_value-b.si_value,a.dimension};}
Quantity operator*(const Quantity&a,const Quantity&b){return {a.si_value*b.si_value,a.dimension+b.dimension};}
Quantity operator/(const Quantity&a,const Quantity&b){return {a.si_value/b.si_value,a.dimension-b.dimension};}
Quantity scale(const Quantity&a,const Rational&s){return {a.si_value*s,a.dimension};}
std::string QuantityInterval::canonical()const{return si_value.str()+" SI ["+dimension.str()+"]";}

FormulaResult evaluate_formula(std::string_view id,const std::vector<Quantity>&a,unsigned d){
  if(id=="kinetic_energy"){arity(id,a,2);require_dimension(a[0],Mass,"mass");require_dimension(a[1],Speed,"speed");return {exact_q(a[0].si_value*a[1].si_value*a[1].si_value/Rational(2),Energy),std::string(id)};}
  if(id=="newton_force"){arity(id,a,4);require_dimension(a[0],Dimension{3,-1,-2,0,0},"G");require_dimension(a[1],Mass,"m1");require_dimension(a[2],Mass,"m2");require_dimension(a[3],Length,"r");return {interval_q(Interval(a[0].si_value*a[1].si_value*a[2].si_value)/(Interval(a[3].si_value)*Interval(a[3].si_value)),Force),std::string(id)};}
  if(id=="escape_speed"){arity(id,a,3);require_dimension(a[0],Dimension{3,-1,-2,0,0},"G");require_dimension(a[1],Mass,"M");require_dimension(a[2],Length,"r");return {interval_q(sqrt_interval(Interval(Rational(2)*a[0].si_value*a[1].si_value/a[2].si_value),d),Speed),std::string(id)};}
  if(id=="circular_orbit_period"){arity(id,a,3);require_dimension(a[0],Length,"r");require_dimension(a[1],Dimension{3,-1,-2,0,0},"G");require_dimension(a[2],Mass,"M");Interval pi(pi_lo(),pi_hi());Interval r(a[0].si_value);auto x=(r*r*r)/Interval(a[1].si_value*a[2].si_value);return {interval_q(Interval(Rational(2))*pi*sqrt_interval(x,d),Time),std::string(id)};}
  if(id=="hubble_flow"){arity(id,a,2);require_dimension(a[0],Frequency,"H");require_dimension(a[1],Length,"distance");return {exact_q(a[0].si_value*a[1].si_value,Speed),std::string(id)};}
  if(id=="cosmological_redshift"){arity(id,a,2);require_dimension(a[0],Dimless,"a_emit");require_dimension(a[1],Dimless,"a_obs");if(a[0].si_value<=Rational(0))throw std::domain_error("a_emit");return {exact_q(a[1].si_value/a[0].si_value-Rational(1),Dimless),std::string(id)};}
  if(id=="critical_density"){arity(id,a,2);require_dimension(a[0],Frequency,"H");require_dimension(a[1],Dimension{3,-1,-2,0,0},"G");Interval pi(pi_lo(),pi_hi());return {interval_q(Interval(Rational(3)*a[0].si_value*a[0].si_value)/(Interval(Rational(8)*a[1].si_value)*pi),Density),std::string(id)};}
  if(id=="electric_potential"){arity(id,a,3);require_dimension(a[0],Dimension{3,1,-2,-2,0},"k");require_dimension(a[1],Charge,"q");require_dimension(a[2],Length,"r");return {exact_q(a[0].si_value*a[1].si_value/a[2].si_value,ElectricPotential),std::string(id)};}
  if(id=="lorentz_force_component"){arity(id,a,3);require_dimension(a[0],Charge,"q");require_dimension(a[1],ElectricField,"E_component");require_dimension(a[2],ElectricField,"v_cross_B_component");return {exact_q(a[0].si_value*(a[1].si_value+a[2].si_value),Force),std::string(id)};}
  if(id=="magnetic_energy_density"){arity(id,a,2);require_dimension(a[0],MagneticField,"B");require_dimension(a[1],Permeability,"mu0");return {exact_q(a[0].si_value*a[0].si_value/(Rational(2)*a[1].si_value),Pressure),std::string(id)};}
  if(id=="blackbody_power"){arity(id,a,3);require_dimension(a[0],Area,"area");require_dimension(a[1],Dimension{0,1,-3,0,-4},"sigma");require_dimension(a[2],Temperature,"temperature");Rational t2=a[2].si_value*a[2].si_value;return {exact_q(a[0].si_value*a[1].si_value*t2*t2,Dimension{2,1,-3,0,0}),std::string(id)};}
  if(id=="sound_speed"){arity(id,a,3);require_dimension(a[0],Dimless,"gamma");require_dimension(a[1],Pressure,"pressure");require_dimension(a[2],Density,"density");return {interval_q(sqrt_interval(Interval(a[0].si_value*a[1].si_value/a[2].si_value),d),Speed),std::string(id)};}
  if(id=="alfven_speed"){arity(id,a,3);require_dimension(a[0],MagneticField,"B");require_dimension(a[1],Permeability,"mu0");require_dimension(a[2],Density,"density");return {interval_q(sqrt_interval(Interval(a[0].si_value*a[0].si_value/(a[1].si_value*a[2].si_value)),d),Speed),std::string(id)};}
  if(id=="reynolds_number"){arity(id,a,4);require_dimension(a[0],Density,"density");require_dimension(a[1],Speed,"speed");require_dimension(a[2],Length,"length");require_dimension(a[3],Dimension{-1,1,-1,0,0},"dynamic_viscosity");return {exact_q(a[0].si_value*a[1].si_value*a[2].si_value/a[3].si_value,Dimless),std::string(id)};}
  if(id=="mach_number"){arity(id,a,2);require_dimension(a[0],Speed,"speed");require_dimension(a[1],Speed,"sound_speed");return {exact_q(a[0].si_value/a[1].si_value,Dimless),std::string(id)};}
  if(id=="uncertainty_lower_bound"){arity(id,a,1);require_dimension(a[0],Dimension{2,1,-1,0,0},"hbar");return {exact_q(a[0].si_value/Rational(2),Dimension{2,1,-1,0,0}),std::string(id)};}
  if(id=="compton_wavelength"){arity(id,a,3);require_dimension(a[0],Dimension{2,1,-1,0,0},"h");require_dimension(a[1],Mass,"mass");require_dimension(a[2],Speed,"c");return {exact_q(a[0].si_value/(a[1].si_value*a[2].si_value),Length),std::string(id)};}
  if(id=="bohr_radius"){arity(id,a,4);require_dimension(a[0],Permittivity,"four_pi_eps0");require_dimension(a[1],Dimension{2,1,-1,0,0},"hbar");require_dimension(a[2],Mass,"electron_mass");require_dimension(a[3],Charge,"elementary_charge");return {exact_q(a[0].si_value*a[1].si_value*a[1].si_value/(a[2].si_value*a[3].si_value*a[3].si_value),Length),std::string(id)};}
  if(id=="hydrogen_ground_state_energy"){arity(id,a,4);require_dimension(a[0],Mass,"electron_mass");require_dimension(a[1],Charge,"elementary_charge");require_dimension(a[2],Permittivity,"four_pi_eps0");require_dimension(a[3],Dimension{2,1,-1,0,0},"hbar");Rational e2=a[1].si_value*a[1].si_value;Rational denom=Rational(2)*a[2].si_value*a[2].si_value*a[3].si_value*a[3].si_value;return {exact_q(a[0].si_value*e2*e2/denom,Energy),std::string(id)};}
  if(id=="nuclear_mass_defect"){arity(id,a,2);require_dimension(a[0],Mass,"constituent_mass");require_dimension(a[1],Mass,"bound_mass");return {exact_q(a[0].si_value-a[1].si_value,Mass),std::string(id)};}
  if(id=="schwarzschild_time_dilation"){arity(id,a,4);require_dimension(a[0],Dimension{3,-1,-2,0,0},"G");require_dimension(a[1],Mass,"M");require_dimension(a[2],Speed,"c");require_dimension(a[3],Length,"r");Rational x=Rational(1)-Rational(2)*a[0].si_value*a[1].si_value/(a[2].si_value*a[2].si_value*a[3].si_value);return {interval_q(sqrt_interval(Interval(x),d),Dimless),std::string(id)};}
  if(id=="bekenstein_hawking_entropy"){arity(id,a,5);require_dimension(a[0],EntropyDim,"kB");require_dimension(a[1],Speed,"c");require_dimension(a[2],Area,"area");require_dimension(a[3],Dimension{3,-1,-2,0,0},"G");require_dimension(a[4],Dimension{2,1,-1,0,0},"hbar");Rational c3=a[1].si_value*a[1].si_value*a[1].si_value;return {exact_q(a[0].si_value*c3*a[2].si_value/(Rational(4)*a[3].si_value*a[4].si_value),EntropyDim),std::string(id)};}
  if(id=="jeans_length"){arity(id,a,3);require_dimension(a[0],Speed,"sound_speed");require_dimension(a[1],Dimension{3,-1,-2,0,0},"G");require_dimension(a[2],Density,"density");Interval pi(pi_lo(),pi_hi());return {interval_q(sqrt_interval(pi*Interval(a[0].si_value*a[0].si_value/(a[1].si_value*a[2].si_value)),d),Length),std::string(id)};}
  if(id=="planck_length"){arity(id,a,3);require_dimension(a[0],Dimension{2,1,-1,0,0},"hbar");require_dimension(a[1],Dimension{3,-1,-2,0,0},"G");require_dimension(a[2],Speed,"c");Rational c3=a[2].si_value*a[2].si_value*a[2].si_value;return {interval_q(sqrt_interval(Interval(a[0].si_value*a[1].si_value/c3),d),Length),std::string(id)};}
  if(id=="planck_mass"){arity(id,a,3);require_dimension(a[0],Dimension{2,1,-1,0,0},"hbar");require_dimension(a[1],Speed,"c");require_dimension(a[2],Dimension{3,-1,-2,0,0},"G");return {interval_q(sqrt_interval(Interval(a[0].si_value*a[1].si_value/a[2].si_value),d),Mass),std::string(id)};}
  throw std::invalid_argument("unknown formula id: "+std::string(id));
}
std::vector<std::string> formula_catalogue_json_lines(){return {
  "{\"id\":\"kinetic_energy\",\"unit_checked\":true}","{\"id\":\"newton_force\",\"unit_checked\":true}","{\"id\":\"escape_speed\",\"unit_checked\":true}","{\"id\":\"circular_orbit_period\",\"unit_checked\":true}","{\"id\":\"hubble_flow\",\"unit_checked\":true}","{\"id\":\"cosmological_redshift\",\"unit_checked\":true}","{\"id\":\"critical_density\",\"unit_checked\":true}","{\"id\":\"electric_potential\",\"unit_checked\":true}","{\"id\":\"lorentz_force_component\",\"unit_checked\":true}","{\"id\":\"magnetic_energy_density\",\"unit_checked\":true}","{\"id\":\"blackbody_power\",\"unit_checked\":true}","{\"id\":\"sound_speed\",\"unit_checked\":true}","{\"id\":\"alfven_speed\",\"unit_checked\":true}","{\"id\":\"reynolds_number\",\"unit_checked\":true}","{\"id\":\"mach_number\",\"unit_checked\":true}","{\"id\":\"uncertainty_lower_bound\",\"unit_checked\":true}","{\"id\":\"compton_wavelength\",\"unit_checked\":true}","{\"id\":\"bohr_radius\",\"unit_checked\":true}","{\"id\":\"hydrogen_ground_state_energy\",\"unit_checked\":true}","{\"id\":\"nuclear_mass_defect\",\"unit_checked\":true}","{\"id\":\"schwarzschild_time_dilation\",\"unit_checked\":true}","{\"id\":\"bekenstein_hawking_entropy\",\"unit_checked\":true}","{\"id\":\"jeans_length\",\"unit_checked\":true}","{\"id\":\"planck_length\",\"unit_checked\":true}","{\"id\":\"planck_mass\",\"unit_checked\":true}"};}
}  // namespace surreal
