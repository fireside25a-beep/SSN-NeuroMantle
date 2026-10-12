#include "surreal/field_kernels.hpp"
#include "surreal/exact.hpp"
#include "surreal/hash.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

namespace surreal {
namespace {
std::string hash_i64(const std::vector<std::int64_t>& a){return hex(sha256(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(a.data()),a.size()*sizeof(std::int64_t))));}
void check_dims(std::size_t n,int nx,int ny,int nz){
  if(nx<=0||ny<=0||nz<=0)throw std::invalid_argument("field dimensions");
  constexpr std::size_t max_cells=10'000'000U;
  std::size_t cells=static_cast<std::size_t>(nx);
  for(int d:{ny,nz}){const auto ud=static_cast<std::size_t>(d);if(cells>max_cells/ud)throw std::invalid_argument("field cell bound");cells*=ud;}
  if(cells!=n||cells>static_cast<std::size_t>(std::numeric_limits<int>::max()))throw std::invalid_argument("field dimensions");
}
std::uint64_t magnitude(std::int64_t v){return v<0?static_cast<std::uint64_t>(-(v+1))+1ULL:static_cast<std::uint64_t>(v);}
void require_magnitude(const std::vector<std::int64_t>&v,unsigned divisor,std::string_view op){const auto limit=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())/divisor;for(auto x:v)if(magnitude(x)>limit)throw std::overflow_error(std::string(op)+": int64 safety envelope exceeded");}
std::string divergence_pair_hash(const std::vector<std::int64_t>& ex,const std::vector<std::int64_t>& ey,const std::vector<std::int64_t>& ez,const std::vector<std::int64_t>& bx,const std::vector<std::int64_t>& by,const std::vector<std::int64_t>& bz,int nx,int ny,int nz){for(auto*v:{&ex,&ey,&ez,&bx,&by,&bz})require_magnitude(*v,6,"divergence");std::vector<std::int64_t>de(ex.size()),db(ex.size());surreal_divergence_i64(ex.data(),ey.data(),ez.data(),de.data(),nx,ny,nz);surreal_divergence_i64(bx.data(),by.data(),bz.data(),db.data(),nx,ny,nz);return hash_join({hash_i64(de),hash_i64(db)});}
}
FieldDivergenceCertificate maxwell_step_certified(std::vector<std::int64_t>&ex,std::vector<std::int64_t>&ey,std::vector<std::int64_t>&ez,std::vector<std::int64_t>&bx,std::vector<std::int64_t>&by,std::vector<std::int64_t>&bz,int nx,int ny,int nz){
  check_dims(ex.size(),nx,ny,nz);for(auto*v:{&ey,&ez,&bx,&by,&bz})if(v->size()!=ex.size())throw std::invalid_argument("field size mismatch");for(auto*v:{&ex,&ey,&ez,&bx,&by,&bz})require_magnitude(*v,128,"maxwell");
  const auto before=divergence_pair_hash(ex,ey,ez,bx,by,bz,nx,ny,nz);
  auto nex=ex,ney=ey,nez=ez,nbx=bx,nby=by,nbz=bz;
  surreal_maxwell_leap_i64(nex.data(),ney.data(),nez.data(),nbx.data(),nby.data(),nbz.data(),nx,ny,nz);
  const auto after=divergence_pair_hash(nex,ney,nez,nbx,nby,nbz,nx,ny,nz);
  const bool pass=before==after;
  if(pass){ex.swap(nex);ey.swap(ney);ez.swap(nez);bx.swap(nbx);by.swap(nby);bz.swap(nbz);}
  return {pass,before,after};
}
bool scalar_needs_refinement(const std::vector<std::int64_t>&field,int nx,int ny,int nz,std::int64_t threshold){check_dims(field.size(),nx,ny,nz);if(threshold<0)throw std::invalid_argument("negative threshold");require_magnitude(field,12,"laplacian");std::vector<std::int64_t>lap(field.size());surreal_laplacian_i64(field.data(),lap.data(),nx,ny,nz);for(auto v:lap)if(magnitude(v)>static_cast<std::uint64_t>(threshold))return true;return false;}
void kick_drift_checked(std::vector<std::int64_t>&pos,std::vector<std::int64_t>&vel,const std::vector<std::int64_t>&acc,std::int64_t dt){if(pos.size()!=vel.size()||pos.size()!=acc.size()||pos.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()))throw std::invalid_argument("kick-drift sizes");const BigInt lo=std::numeric_limits<std::int64_t>::min(),hi=std::numeric_limits<std::int64_t>::max();for(std::size_t i=0;i<pos.size();++i){BigInt nv=BigInt(vel[i])+BigInt(acc[i])*dt;BigInt np=BigInt(pos[i])+nv*dt;if(nv<lo||nv>hi||np<lo||np>hi)throw std::overflow_error("kick-drift int64 overflow");}surreal_kick_drift_i64(pos.data(),vel.data(),acc.data(),dt,static_cast<int>(pos.size()));}
}  // namespace surreal
