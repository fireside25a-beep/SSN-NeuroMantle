#include "surreal/entropy.hpp"
#include "surreal/hash.hpp"
#include <charconv>
#include <sstream>
#include <stdexcept>

namespace surreal {
namespace {
constexpr std::uint32_t M0=0xD2511F53u,M1=0xCD9E8D57u,W0=0x9E3779B9u,W1=0xBB67AE85u;
std::uint64_t derive_stream(std::string_view root,std::string_view domain,std::uint64_t stream,std::uint64_t counter){
  auto d=sha256(hash_join({root,domain,std::to_string(stream),std::to_string(counter)}));
  std::uint64_t v=0;for(int i=0;i<8;++i)v=(v<<8)|d[static_cast<std::size_t>(i)];return v;
}
bool lower_hex64(std::string_view s){if(s.size()!=64)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
std::uint64_t parse_u64(std::string_view s){std::uint64_t v=0;auto [p,ec]=std::from_chars(s.data(),s.data()+s.size(),v);if(ec!=std::errc{}||p!=s.data()+s.size())throw std::invalid_argument("invalid entropy integer");return v;}
}
std::array<std::uint32_t,4> philox4x32_10(std::array<std::uint32_t,4> c,std::array<std::uint32_t,2> k){
  for(int round=0;round<10;++round){
    std::uint64_t p0=std::uint64_t(M0)*c[0],p1=std::uint64_t(M1)*c[2];
    std::array<std::uint32_t,4> n={std::uint32_t(p1>>32)^c[1]^k[0],std::uint32_t(p1),std::uint32_t(p0>>32)^c[3]^k[1],std::uint32_t(p0)};
    c=n;if(round!=9){k[0]+=W0;k[1]+=W1;}
  }return c;
}
EntropyLedger::EntropyLedger(std::optional<std::uint64_t>seed,std::uint64_t stream):seed_(seed),stream_id_(stream),initial_root_(hex(sha256("SURREAL_ENTROPY_V2"))),root_(initial_root_){}
std::uint64_t EntropyLedger::draw(){
  if(draws_.size()>=kMaxEntropyAuditDraws)throw std::runtime_error("entropy audit draw bound exceeded");
  std::uint64_t v=0;
  if(seed_){
    std::array<std::uint32_t,4> c={std::uint32_t(counter_),std::uint32_t(counter_>>32),std::uint32_t(stream_id_),std::uint32_t(stream_id_>>32)};
    std::array<std::uint32_t,2> k={std::uint32_t(*seed_),std::uint32_t(*seed_>>32)};auto r=philox4x32_10(c,k);v=(std::uint64_t(r[1])<<32)|r[0];
  }else if(surreal_getrandom_u64(&v)!=0)throw std::runtime_error("getrandom failed");
  ++counter_;draws_.push_back(v);root_=hash_join({root_,std::to_string(counter_),std::to_string(stream_id_),std::to_string(v)});return v;
}
EntropyLedger EntropyLedger::fork_independent(std::string_view domain) const {
  if(domain.empty())throw std::invalid_argument("empty entropy fork domain");
  if(!seed_)throw std::logic_error("independent deterministic fork requires replay seed");
  EntropyLedger child=*this;
  child.stream_id_=derive_stream(root_,domain,stream_id_,counter_);
  child.counter_=0;
  child.draws_.clear();
  child.initial_root_=hash_join({root_,"entropy_fork",domain,std::to_string(child.stream_id_)});
  child.root_=child.initial_root_;
  return child;
}
std::string EntropyLedger::audit_log() const {
  std::ostringstream o;o<<"SURREAL_ENTROPY_LOG_V1\n"<<"replayable="<<(replayable()?1:0)<<"\n"<<"initial_root="<<initial_root_<<"\n"<<"stream="<<stream_id_<<"\n"<<"count="<<draws_.size()<<"\n";
  for(std::size_t i=0;i<draws_.size();++i)o<<"draw="<<i<<":"<<draws_[i]<<"\n";
  o<<"root="<<root_<<"\n";return o.str();
}
bool verify_entropy_log(std::string_view log,std::string_view expected_root,std::string*error){
  try{
    std::istringstream in{std::string(log)};std::string line;
    auto next=[&](){if(!std::getline(in,line))throw std::runtime_error("truncated entropy log");return line;};
    if(next()!="SURREAL_ENTROPY_LOG_V1")throw std::runtime_error("entropy log schema");
    auto replay=next();if(replay!="replayable=0"&&replay!="replayable=1")throw std::runtime_error("entropy replay flag");
    auto initial=next();if(!initial.starts_with("initial_root=")||!lower_hex64(initial.substr(13)))throw std::runtime_error("entropy initial root");std::string root=initial.substr(13);
    auto stream_line=next();if(!stream_line.starts_with("stream="))throw std::runtime_error("entropy stream");auto stream=parse_u64(stream_line.substr(7));
    auto count_line=next();if(!count_line.starts_with("count="))throw std::runtime_error("entropy count");auto count=parse_u64(count_line.substr(6));if(count>kMaxEntropyAuditDraws)throw std::runtime_error("entropy count bound");
    for(std::uint64_t i=0;i<count;++i){auto dline=next();if(!dline.starts_with("draw="))throw std::runtime_error("entropy draw line");auto colon=dline.find(':',5);if(colon==std::string::npos)throw std::runtime_error("entropy draw framing");if(parse_u64(std::string_view(dline).substr(5,colon-5))!=i)throw std::runtime_error("entropy draw index");auto v=parse_u64(std::string_view(dline).substr(colon+1));root=hash_join({root,std::to_string(i+1),std::to_string(stream),std::to_string(v)});}
    auto root_line=next();if(!root_line.starts_with("root=")||!lower_hex64(root_line.substr(5)))throw std::runtime_error("entropy root line");if(root_line.substr(5)!=root)throw std::runtime_error("entropy chain mismatch");if(root!=expected_root)throw std::runtime_error("entropy expected root mismatch");
    while(std::getline(in,line)) {
      if(!line.empty()) throw std::runtime_error("trailing entropy log data");
    }
    return true;
  }catch(const std::exception&e){if(error)*error=e.what();return false;}
}
}  // namespace surreal
