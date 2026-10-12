#include "surreal/physics.hpp"
#include "surreal/hash.hpp"
#include "canonical_internal.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace surreal {
namespace {
Interval mul(const Interval&a,const Rational&b){return a*Interval(b);}
Vec3I addv(const Vec3I&a,const Vec3I&b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3I subv(const Vec3I&a,const Vec3I&b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3I scalev(const Vec3I&a,const Rational&s){return {mul(a.x,s),mul(a.y,s),mul(a.z,s)};}
Vec3I scalev(const Vec3I&a,const Interval&s){return {a.x*s,a.y*s,a.z*s};}
Vec3I crossv(const Vec3I&a,const Vec3I&b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Interval norm2(const Vec3I&a){return square(a.x)+square(a.y)+square(a.z);}
Vec3I total_momentum(const BodySystem&s){Vec3I p{};for(const auto&b:s.bodies)p=addv(p,scalev(b.velocity,b.mass));return p;}
Vec3I total_angular(const BodySystem&s){Vec3I l{};for(const auto&b:s.bodies)l=addv(l,crossv(b.position,scalev(b.velocity,b.mass)));return l;}
Rational total_mass_energy(const BodySystem&s){Rational m=s.radiation_energy;for(const auto&b:s.bodies)m=m+b.mass;return m;}
Rational total_charge(const BodySystem&s){Rational q(0);for(const auto&b:s.bodies)q=q+b.charge;return q;}
bool delta_contains_zero(const Interval&a,const Interval&b){return (b-a).contains_zero();}
std::string vstr(const Vec3I&v){return v.x.str()+","+v.y.str()+","+v.z.str();}
std::string body_canonical(const Body&b){std::string s="SURREAL_BODY_V2\n";detail::append_field(s,"id",b.id);detail::append_field(s,"mass",b.mass.str());detail::append_field(s,"charge",b.charge.str());detail::append_field(s,"position",vstr(b.position));detail::append_field(s,"velocity",vstr(b.velocity));return s;}
std::vector<Vec3I> pair_forces(const BodySystem&s,const Rational&coupling,const Rational&soft2,bool charge_weight,unsigned digits){
  std::vector<Vec3I> f(s.bodies.size());
  for(std::size_t i=0;i<s.bodies.size();++i)for(std::size_t j=i+1;j<s.bodies.size();++j){
    Vec3I r=subv(s.bodies[j].position,s.bodies[i].position); Interval r2=norm2(r)+Interval(soft2); if(r2.lo<=Rational(0))throw std::domain_error("nonpositive softened radius"); Interval root=sqrt_interval(r2,digits); Interval denom=r2*root; Rational weight=charge_weight?s.bodies[i].charge*s.bodies[j].charge:s.bodies[i].mass*s.bodies[j].mass; Interval scalar=Interval(coupling*weight)/denom; Vec3I fij=scalev(r,scalar); f[i]=addv(f[i],fij); f[j]=subv(f[j],fij);
  }
  return f;
}
void apply_kick(BodySystem&out,const std::vector<Vec3I>&f,const Rational&dt){for(std::size_t i=0;i<out.bodies.size();++i){if(out.bodies[i].mass.is_zero())continue;out.bodies[i].velocity=addv(out.bodies[i].velocity,scalev(f[i],dt/out.bodies[i].mass));}}
}
std::string BodySystem::canonical()const{std::string s="SURREAL_BODY_SYSTEM_V2\n";detail::append_field(s,"radiation",radiation_energy.str());detail::append_field(s,"body_count",std::to_string(bodies.size()));for(const auto&b:bodies)detail::append_field(s,"body",body_canonical(b));return s;}
std::string InvariantCertificate::hash()const{std::string s="SURREAL_INVARIANT_V2\n";detail::append_field(s,"pass",pass?"1":"0");for(const auto&[k,v]:claims){detail::append_field(s,"claim_key",k);detail::append_field(s,"claim_value",v);}return hex(sha256(s));}
InvariantCertificate body_invariants(const BodySystem&before,const BodySystem&after){
  InvariantCertificate c; auto pb=total_momentum(before),pa=total_momentum(after),lb=total_angular(before),la=total_angular(after); bool mass=total_mass_energy(before)==total_mass_energy(after);bool charge=total_charge(before)==total_charge(after);bool mom=delta_contains_zero(pb.x,pa.x)&&delta_contains_zero(pb.y,pa.y)&&delta_contains_zero(pb.z,pa.z);bool ang=delta_contains_zero(lb.x,la.x)&&delta_contains_zero(lb.y,la.y)&&delta_contains_zero(lb.z,la.z);c.pass=mass&&charge&&mom&&ang;c.claims={{"mass_energy",mass?"preserved":"violated"},{"charge",charge?"preserved":"violated"},{"momentum",mom?"enclosed":"violated"},{"angular_momentum",ang?"enclosed":"violated"}};return c;
}
CosmologyParams cosmology_world(std::string_view w){if(w=="lcdm")return {Rational(1,BigInt(10000)),Rational(3,BigInt(10)),Rational(0),Rational(6999,BigInt(10000))};if(w=="eds")return {Rational(0),Rational(1),Rational(0),Rational(0)};if(w=="radiation")return {Rational(1),Rational(0),Rational(0),Rational(0)};throw std::invalid_argument("unknown cosmology world");}
CosmologyState friedmann_step(const CosmologyState&s,const Rational&dt,unsigned digits){auto p=cosmology_world(s.world);if(s.scale_factor.lo<=Rational(0))throw std::domain_error("nonpositive scale factor");Interval a=s.scale_factor,a2=a*a,a3=a2*a,a4=a2*a2;Interval h2=Interval(p.omega_r)/a4+Interval(p.omega_m)/a3+Interval(p.omega_k)/a2+Interval(p.omega_l);Interval h=sqrt_interval(h2,digits);return {a+mul(a*h,dt),s.world};}
LawResult gravity_kdk(const BodySystem&in,const Rational&dt,const Rational&G,const Rational&soft2,unsigned digits){BodySystem out=in;auto f1=pair_forces(out,G,soft2,false,digits);apply_kick(out,f1,dt/Rational(2));for(auto&b:out.bodies)b.position=addv(b.position,scalev(b.velocity,dt));auto f2=pair_forces(out,G,soft2,false,digits);apply_kick(out,f2,dt/Rational(2));auto cert=body_invariants(in,out);return {out,cert,"softened_newton_kdk"};}
LawResult coulomb_kick(const BodySystem&in,const Rational&dt,const Rational&k,const Rational&soft2,unsigned digits){BodySystem out=in;auto f=pair_forces(out,k,soft2,true,digits);apply_kick(out,f,dt);auto cert=body_invariants(in,out);return {out,cert,"coulomb_pair_kick"};}
Body photon_propagate(const Body&photon,const Vec3I&direction,const Rational&dt){if(!photon.mass.is_zero()||!photon.charge.is_zero())throw std::invalid_argument("photon packet must be massless and neutral");if(dt<Rational(0))throw std::invalid_argument("negative photon timestep");auto n2=norm2(direction);if(n2.lo!=Rational(1)||n2.hi!=Rational(1))throw std::invalid_argument("photon direction must have exact unit norm");Body out=photon;out.velocity=direction;out.position=addv(out.position,scalev(direction,dt));return out;}
LawResult rest_mass_to_radiation(const BodySystem&in,std::size_t i,const Rational&amount){if(i>=in.bodies.size())throw std::out_of_range("body index");if(amount<Rational(0)||amount>in.bodies[i].mass)throw std::invalid_argument("mass conversion amount");BodySystem out=in;out.bodies[i].mass=out.bodies[i].mass-amount;out.radiation_energy=out.radiation_energy+amount;InvariantCertificate c;bool mass=total_mass_energy(in)==total_mass_energy(out);bool charge=total_charge(in)==total_charge(out);c.pass=mass&&charge;c.claims={{"mass_energy",mass?"preserved":"violated"},{"charge",charge?"preserved":"violated"},{"momentum","not_represented_for_radiation_packet"}};return {out,c,"rest_mass_to_radiation_c_equals_1"};}
LawResult stochastic_two_body_decay(const BodySystem&in,std::size_t i,Rational probability,EntropyLedger&e){if(i>=in.bodies.size())throw std::out_of_range("body index");if(probability<Rational(0)||probability>Rational(1))throw std::invalid_argument("probability");std::uint64_t u=e.draw();BigInt threshold=(BigInt(1)<<64)*probability.num()/probability.den();if(BigInt(u)>=threshold)return {in,body_invariants(in,in),"no_decay"};BodySystem out=in;Body parent=out.bodies[i];Rational left=parent.mass/Rational(2),right=parent.mass-left;Rational qleft=parent.charge/Rational(2),qright=parent.charge-qleft;parent.id+=".a";parent.mass=left;parent.charge=qleft;Body b=parent;b.id=out.bodies[i].id+".b";b.mass=right;b.charge=qright;out.bodies[i]=parent;out.bodies.push_back(b);auto c=body_invariants(in,out);return {out,c,"two_body_decay"};}
std::vector<std::string> law_catalogue_json_lines(){return {
  "{\"id\":\"cosmology.friedmann\",\"kind\":\"executed\",\"numeric\":\"rational_interval\"}",
  "{\"id\":\"mechanics.gravity_kdk\",\"kind\":\"executed\",\"numeric\":\"rational_interval\"}",
  "{\"id\":\"electromagnetism.coulomb\",\"kind\":\"executed\",\"numeric\":\"rational_interval\"}",
  "{\"id\":\"radiation.photon_inertial\",\"kind\":\"executed\",\"numeric\":\"exact_rational\"}",
  "{\"id\":\"decay.two_body_entropy\",\"kind\":\"executed\",\"numeric\":\"committed_entropy\"}",
  "{\"id\":\"mass.radiation_bookkeeping\",\"kind\":\"executed\",\"numeric\":\"exact_rational\"}",
  "{\"id\":\"field.maxwell_periodic\",\"kind\":\"executed\",\"numeric\":\"exact_int64\"}",
  "{\"id\":\"field.scalar_refinement\",\"kind\":\"executed\",\"numeric\":\"exact_int64\"}",
  "{\"id\":\"formula.physics_atlas\",\"kind\":\"certified_reference\",\"numeric\":\"unit_checked_rational_interval\"}"
};}
}  // namespace surreal
