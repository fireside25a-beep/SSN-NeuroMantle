#include "surreal/universe.hpp"
#include "surreal/field_kernels.hpp"
#include "surreal/hash.hpp"
#include "surreal/rewrite.hpp"
#include "surreal/version.hpp"
#include "canonical_internal.hpp"
#include "io_internal.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <set>
#include <sstream>
#include <stdexcept>

namespace surreal {
namespace {
constexpr std::size_t kMaxConfigBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaxReportBytes = 1U * 1024U * 1024U;
constexpr std::size_t kMaxEventsBytes = 256U * 1024U * 1024U;
constexpr std::size_t kMaxEntropyLogBytes = 128U * 1024U * 1024U;
constexpr std::size_t kMaxManifestBytes = 64U * 1024U;
constexpr std::uint64_t kMaxFieldCells = 10'000'000ULL;
constexpr std::uint64_t kMaxReferenceFieldWork = 100'000'000ULL;
constexpr std::uint64_t kMaxReferencePairWork = 50'000'000ULL;
constexpr std::size_t kMaxBodies = 1024;
constexpr std::size_t kMaxConfigLineBytes = 16U * 1024U;
constexpr std::size_t kMaxNumericTokenBytes = 4096U;

const std::array<std::string_view,10> kBuiltInLaws = {
  "universe.genesis", "universe.join", "cosmology.friedmann", "mechanics.gravity_kdk",
  "electromagnetism.coulomb", "radiation.photon_inertial", "decay.two_body_entropy",
  "mass.radiation_bookkeeping", "field.maxwell_periodic", "field.scalar_refinement"
};

bool valid_identifier(std::string_view s){
  if(s.empty()||s.size()>128)return false;
  for(unsigned char c:s){
    const bool ascii_alnum=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9');
    if(!(ascii_alnum||c=='.'||c=='_'||c=='-'||c==':'))return false;
  }
  return true;
}
template<class T>T parse_integral(std::string_view s,std::string_view what){
  if(s.empty()||s.size()>64)throw std::invalid_argument(std::string(what)+" integer bound");
  T v{};auto [end,ec]=std::from_chars(s.data(),s.data()+s.size(),v);
  if(ec!=std::errc{}||end!=s.data()+s.size())throw std::invalid_argument("invalid "+std::string(what));
  return v;
}
bool lower_hex64(std::string_view s){if(s.size()!=64)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
std::vector<std::string> normalized_parents(std::vector<std::string> p){std::sort(p.begin(),p.end());p.erase(std::unique(p.begin(),p.end()),p.end());return p;}
InvariantCertificate pass_cert(std::string k,std::string v){InvariantCertificate c;c.pass=true;c.claims.emplace(std::move(k),std::move(v));return c;}
std::string events_text(const CausalGraph&g){std::string s="SURREAL_EVENTS_V2\n";detail::append_field(s,"event_count",std::to_string(g.size()));for(const auto&[id,e]:g.events()){detail::append_field(s,"event_id",id);detail::append_field(s,"event",e.canonical());}return s;}
std::vector<std::string> split(const std::string&s,char d){std::vector<std::string>v;std::string cur;std::istringstream in(s);while(std::getline(in,cur,d))v.push_back(cur);return v;}
void apply_sparse(std::vector<std::int64_t>&v,const std::vector<SparseI64Init>&in){for(const auto&e:in){if(e.index>=v.size())throw std::invalid_argument("sparse field index out of range");v[e.index]=e.value;}}
Rational parse_config_rational(std::string_view s){if(s.empty()||s.size()>kMaxNumericTokenBytes)throw std::invalid_argument("config rational token bound");return Rational::parse(s);}
SparseI64Init parse_sparse(std::string_view s){if(s.size()>512)throw std::invalid_argument("sparse field entry bound");auto p=s.find(':');if(p==std::string_view::npos)throw std::invalid_argument("invalid sparse field entry");return {parse_integral<std::size_t>(s.substr(0,p),"sparse index"),parse_integral<std::int64_t>(s.substr(p+1),"sparse value")};}
Body parse_body(const std::string&s){if(s.size()>kMaxConfigLineBytes)throw std::invalid_argument("body config line bound");auto f=split(s,'|');if(f.size()!=9)throw std::invalid_argument("body requires id|m|q|px|py|pz|vx|vy|vz");if(!valid_identifier(f[0]))throw std::invalid_argument("invalid body id");for(std::size_t i=1;i<f.size();++i)if(f[i].size()>kMaxNumericTokenBytes)throw std::invalid_argument("body numeric token bound");return {f[0],parse_config_rational(f[1]),parse_config_rational(f[2]),{Interval(parse_config_rational(f[3])),Interval(parse_config_rational(f[4])),Interval(parse_config_rational(f[5]))},{Interval(parse_config_rational(f[6])),Interval(parse_config_rational(f[7])),Interval(parse_config_rational(f[8]))}};}
void validate_sparse_unique(const std::vector<SparseI64Init>&v,std::string_view name){std::set<std::size_t>seen;for(const auto&e:v)if(!seen.insert(e.index).second)throw std::invalid_argument("duplicate sparse index in "+std::string(name));}
std::map<std::string,std::string> parse_manifest(const std::string&s){
  std::map<std::string,std::string>m;std::istringstream in(s);std::string line;
  while(std::getline(in,line)){
    if(line.empty()) continue;
    auto p=line.find(' ');
    if(p==std::string::npos||p==0||line.find(' ',p+1)!=std::string::npos)throw std::runtime_error("invalid manifest framing");
    auto digest=line.substr(0,p),name=line.substr(p+1);
    if(!lower_hex64(digest))throw std::runtime_error("invalid manifest digest");
    if(name.empty()||name.find('/')!=std::string::npos||name.find('\\')!=std::string::npos||name.find("..")!=std::string::npos)throw std::runtime_error("invalid manifest artifact name");
    if(m.contains(name)) throw std::runtime_error("duplicate manifest artifact");
    m.emplace(std::move(name),std::move(digest));
  }
  return m;
}
void require_clean_output_dir(const std::filesystem::path&out){
  std::error_code ec;auto st=std::filesystem::symlink_status(out,ec);
  if(!ec&&std::filesystem::exists(st)){
    if(std::filesystem::is_symlink(st)||!std::filesystem::is_directory(st))throw std::runtime_error("output path is not a real directory");
    if(!std::filesystem::is_empty(out))throw std::runtime_error("output directory must be empty");
  }else{
    ec.clear();if(!std::filesystem::create_directories(out,ec)&&ec)throw std::runtime_error("cannot create output directory: "+ec.message());
  }
}
void verify_directory_shape(const std::filesystem::path&p){
  const std::set<std::string> expected={"MANIFEST.sha256","config.txt","entropy.log","events.log","report.txt"};std::set<std::string>seen;
  for(const auto&entry:std::filesystem::directory_iterator(p)){
    auto name=entry.path().filename().string();if(!expected.contains(name))throw std::runtime_error("unexpected run artifact: "+name);
    if(!detail::regular_file_nosymlink(entry.path())) throw std::runtime_error("run artifact is not a regular non-symlink file: "+name);
    seen.insert(name);
  }
  if(seen!=expected)throw std::runtime_error("run artifact set incomplete");
}
}

std::string RegionState::canonical() const {std::string s="SURREAL_REGION_V2\n";detail::append_field(s,"id",id);detail::append_field(s,"kind",kind);detail::append_field(s,"payload",payload);return s;}
std::string RegionState::hash() const {return hex(sha256(canonical()));}

Universe::Universe(std::optional<std::uint64_t> seed):entropy_(seed){for(auto law:kBuiltInLaws)declared_laws_.emplace(law);}
void Universe::put_region(RegionState r){if(!valid_identifier(r.id)||!valid_identifier(r.kind))throw std::invalid_argument("invalid region id/kind");if(regions_.contains(r.id))throw std::invalid_argument("region already exists: "+r.id);auto id=r.id;regions_.emplace(id,std::make_shared<const RegionState>(std::move(r)));}
void Universe::replace_region(RegionState r){if(!valid_identifier(r.id)||!valid_identifier(r.kind))throw std::invalid_argument("invalid region id/kind");auto it=regions_.find(r.id);if(it==regions_.end())throw std::out_of_range("replace unknown region");it->second=std::make_shared<const RegionState>(std::move(r));}
const RegionState& Universe::region(const std::string& id) const {auto it=regions_.find(id);if(it==regions_.end())throw std::out_of_range("region");return *it->second;}
const void* Universe::region_identity(const std::string& id) const {auto it=regions_.find(id);if(it==regions_.end())throw std::out_of_range("region");return it->second.get();}
void Universe::declare_law(std::string law){if(!valid_identifier(law))throw std::invalid_argument("invalid law identifier");declared_laws_.insert(std::move(law));}
bool Universe::law_declared(std::string_view law)const noexcept{return declared_laws_.contains(std::string(law));}
Universe Universe::fork() const {return *this;}
Universe Universe::fork_independent(std::string_view branch_domain) const {Universe out=*this;out.entropy_=entropy_.fork_independent(branch_domain);return out;}
void Universe::perturb_region(const std::string& id,std::string payload){auto cur=region(id);replace_region({cur.id,cur.kind,std::move(payload)});}
std::vector<std::string> Universe::invalidated_future(const std::string& e) const {return causal_.descendants(e);}
std::string Universe::transition(const std::string& rid,const std::string& law,const Rational& t,const std::vector<std::string>& parents,const std::string& params,bool deterministic,const std::function<std::pair<std::string,InvariantCertificate>(const std::string&)>& fn){
  if(!law_declared(law)) throw std::invalid_argument("undeclared law: "+law);
  const auto normalized=normalized_parents(parents);
  (void)causal_.validate_admission(normalized,t);
  const RegionState before=region(rid);
  const auto boundary=causal_boundary_root(causal_,normalized);
  const auto key=hash_join({law,before.hash(),params,boundary});
  const EntropyLedger entropy_checkpoint=entropy_;
  try {
    std::string after_payload;
    InvariantCertificate inv;
    bool reused=false;
    bool memo_insert=false;
    if(deterministic){
      auto it=memo_.find(key);
      if(it!=memo_.end()){
        after_payload=it->second.after_payload;
        inv=it->second.invariant;
        reused=true;
      }else{
        auto v=fn(before.payload);
        if(entropy_.counter()!=entropy_checkpoint.counter()||entropy_.root()!=entropy_checkpoint.root())throw std::runtime_error("deterministic law consumed entropy");
        after_payload=std::move(v.first);
        inv=std::move(v.second);
        memo_insert=true;
      }
    }else{
      auto v=fn(before.payload);
      after_payload=std::move(v.first);
      inv=std::move(v.second);
    }
    if(!inv.pass)throw std::runtime_error("law invariant refused transition");

    RegionState after{before.id,before.kind,after_payload};
    auto next_regions=regions_;
    next_regions[rid]=std::make_shared<const RegionState>(after);
    auto next_causal=causal_;
    auto next_memo=memo_;
    if(memo_insert)next_memo[key]={after_payload,inv};

    Event e;
    e.parents=normalized;e.local_time=t;e.region=rid;e.law=law;e.before_hash=before.hash();e.after_hash=after.hash();e.invariant_hash=inv.hash();e.entropy_root=entropy_.root();e.memo_reuse=reused;
    auto event_id=next_causal.append(std::move(e));

    regions_.swap(next_regions);
    std::swap(causal_,next_causal);
    memo_.swap(next_memo);
    return event_id;
  }catch(...){
    entropy_=entropy_checkpoint;
    throw;
  }
}
std::string Universe::state_root() const {std::string s="SURREAL_STATE_V2\n";for(const auto&[id,r]:regions_){detail::append_field(s,"region_id",id);detail::append_field(s,"region_hash",r->hash());}return hex(sha256(s));}

std::string ReferenceUniverseConfig::canonical() const {
  std::ostringstream o;o<<kReferenceConfigSchema<<"\nsteps="<<steps<<"\nworld="<<world<<"\nfriedmann_dt="<<friedmann_dt.str()<<"\ngravity_dt="<<gravity_dt.str()<<"\ngravity_g="<<gravity_g.str()<<"\nsoftening2="<<softening2.str()<<"\ndecay_probability="<<decay_probability.str()<<"\nscalar_threshold="<<scalar_threshold<<"\nfield_dims="<<nx<<","<<ny<<","<<nz<<"\n";
  for(const auto&b:bodies.bodies)o<<"body="<<b.id<<"|"<<b.mass.str()<<"|"<<b.charge.str()<<"|"<<b.position.x.lo.str()<<"|"<<b.position.y.lo.str()<<"|"<<b.position.z.lo.str()<<"|"<<b.velocity.x.lo.str()<<"|"<<b.velocity.y.lo.str()<<"|"<<b.velocity.z.lo.str()<<"\n";
  auto emit=[&](std::string_view name,const std::vector<SparseI64Init>&v){auto sorted=v;std::sort(sorted.begin(),sorted.end(),[](const auto&a,const auto&b){return a.index<b.index;});for(const auto&e:sorted)o<<name<<"="<<e.index<<":"<<e.value<<"\n";};emit("ex",ex);emit("ey",ey);emit("ez",ez);emit("bx",bx);emit("by",by);emit("bz",bz);emit("scalar",scalar);return o.str();
}
void ReferenceUniverseConfig::validate() const {
  if(steps==0||steps>10000) throw std::invalid_argument("step count");
  (void)cosmology_world(world);
  if(nx<=0||ny<=0||nz<=0) throw std::invalid_argument("field dimensions");
  std::uint64_t cells=static_cast<std::uint64_t>(nx);for(int d:{ny,nz}){if(cells>kMaxFieldCells/static_cast<std::uint64_t>(d))throw std::invalid_argument("field too large");cells*=static_cast<std::uint64_t>(d);}if(cells>kMaxFieldCells)throw std::invalid_argument("field too large");
  if(cells>kMaxReferenceFieldWork/static_cast<std::uint64_t>(steps)) throw std::invalid_argument("reference field work budget exceeded");
  if(friedmann_dt<=Rational(0)||gravity_dt<=Rational(0)) throw std::invalid_argument("nonpositive timestep");
  if(gravity_g<=Rational(0)) throw std::invalid_argument("nonpositive gravity coupling");
  if(softening2<=Rational(0)) throw std::invalid_argument("nonpositive softening2");
  if(decay_probability<Rational(0)||decay_probability>Rational(1)) throw std::invalid_argument("decay probability");
  if(scalar_threshold<0) throw std::invalid_argument("scalar threshold");
  if(bodies.bodies.empty()||bodies.bodies.size()>kMaxBodies) throw std::invalid_argument("body count bound");
  std::set<std::string>ids;for(const auto&b:bodies.bodies){if(!valid_identifier(b.id)||!ids.insert(b.id).second)throw std::invalid_argument("duplicate/invalid body id");if(b.mass<Rational(0))throw std::invalid_argument("negative mass");for(const Interval* q:{&b.position.x,&b.position.y,&b.position.z,&b.velocity.x,&b.velocity.y,&b.velocity.z})if(q->lo!=q->hi)throw std::invalid_argument("reference config body coordinates must be point intervals");}
  const auto nb=static_cast<std::uint64_t>(bodies.bodies.size());const auto pairs=(nb*(nb-1))/2;if(pairs>0&&pairs>kMaxReferencePairWork/static_cast<std::uint64_t>(steps))throw std::invalid_argument("reference pair-work budget exceeded");
  validate_sparse_unique(ex,"ex");validate_sparse_unique(ey,"ey");validate_sparse_unique(ez,"ez");validate_sparse_unique(bx,"bx");validate_sparse_unique(by,"by");validate_sparse_unique(bz,"bz");validate_sparse_unique(scalar,"scalar");
}
ReferenceUniverseConfig default_reference_config(std::size_t steps,std::string world){ReferenceUniverseConfig c;c.steps=steps;c.world=std::move(world);c.bodies.bodies={{"a",Rational(2),Rational(1),{Interval(Rational(-1)),Interval(Rational(0)),Interval(Rational(0))},{Interval(Rational(0)),Interval(Rational(1,BigInt(10))),Interval(Rational(0))}},{"b",Rational(3),Rational(-1),{Interval(Rational(1)),Interval(Rational(0)),Interval(Rational(0))},{Interval(Rational(0)),Interval(Rational(-1,BigInt(15))),Interval(Rational(0))}}};c.ex={{1,1}};c.ey={{7,-1}};c.scalar={{21,7}};c.validate();return c;}
ReferenceUniverseConfig load_reference_config(const std::string&path){ReferenceUniverseConfig c;c.bodies.bodies.clear();c.ex.clear();c.ey.clear();c.ez.clear();c.bx.clear();c.by.clear();c.bz.clear();c.scalar.clear();auto text=detail::read_file_bounded(path,kMaxConfigBytes);std::istringstream in(text);std::string line;bool schema=false;std::set<std::string>once;while(std::getline(in,line)){if(line.size()>kMaxConfigLineBytes)throw std::invalid_argument("config line bound");if(line.empty()||line[0]=='#')continue;if(!schema){if(line!=kReferenceConfigSchema)throw std::invalid_argument("config schema");schema=true;continue;}auto p=line.find('=');if(p==std::string::npos)throw std::invalid_argument("config line");auto k=line.substr(0,p),v=line.substr(p+1);if(k=="body"){c.bodies.bodies.push_back(parse_body(v));continue;}if(k=="ex"){c.ex.push_back(parse_sparse(v));continue;}if(k=="ey"){c.ey.push_back(parse_sparse(v));continue;}if(k=="ez"){c.ez.push_back(parse_sparse(v));continue;}if(k=="bx"){c.bx.push_back(parse_sparse(v));continue;}if(k=="by"){c.by.push_back(parse_sparse(v));continue;}if(k=="bz"){c.bz.push_back(parse_sparse(v));continue;}if(k=="scalar"){c.scalar.push_back(parse_sparse(v));continue;}if(!once.insert(k).second)throw std::invalid_argument("duplicate config key");if(k=="steps")c.steps=parse_integral<std::size_t>(v,"steps");else if(k=="world")c.world=v;else if(k=="friedmann_dt")c.friedmann_dt=parse_config_rational(v);else if(k=="gravity_dt")c.gravity_dt=parse_config_rational(v);else if(k=="gravity_g")c.gravity_g=parse_config_rational(v);else if(k=="softening2")c.softening2=parse_config_rational(v);else if(k=="decay_probability")c.decay_probability=parse_config_rational(v);else if(k=="scalar_threshold")c.scalar_threshold=parse_integral<std::int64_t>(v,"scalar_threshold");else if(k=="field_dims"){auto q=split(v,',');if(q.size()!=3)throw std::invalid_argument("field_dims");c.nx=parse_integral<int>(q[0],"field nx");c.ny=parse_integral<int>(q[1],"field ny");c.nz=parse_integral<int>(q[2],"field nz");}else throw std::invalid_argument("unknown config key: "+k);}if(!schema)throw std::invalid_argument("missing config schema");c.validate();return c;}

std::string RunReport::canonical() const {return std::string("schema=")+std::string(kRunReportSchema)+"\nversion="+std::string(kVersion)+"\nuniverse_root="+universe_root+"\ncausal_root="+causal_root+"\nentropy_root="+entropy_root+"\nrewrite_root="+rewrite_root+"\nconfig_root="+config_root+"\nevents="+std::to_string(events)+"\nworld="+world+"\n";}

RunReport run_universe(const ReferenceUniverseConfig& cfg,const std::string& outdir,std::optional<std::uint64_t> seed){
  cfg.validate();
  std::filesystem::path out(outdir);
  require_clean_output_dir(out);
  Universe u(seed);
  CosmologyState cos{Interval(Rational(1)),cfg.world};
  BodySystem bodies=cfg.bodies;
  const std::size_t n=static_cast<std::size_t>(cfg.nx)*static_cast<std::size_t>(cfg.ny)*static_cast<std::size_t>(cfg.nz);
  std::vector<std::int64_t> ex(n),ey(n),ez(n),bx(n),by(n),bz(n),scalar(n);
  apply_sparse(ex,cfg.ex);apply_sparse(ey,cfg.ey);apply_sparse(ez,cfg.ez);apply_sparse(bx,cfg.bx);apply_sparse(by,cfg.by);apply_sparse(bz,cfg.bz);apply_sparse(scalar,cfg.scalar);

  u.put_region({"cosmos","cosmology",cos.scale_factor.str()+"|"+cfg.world});
  u.put_region({"bodies","body_system",bodies.canonical()});
  u.put_region({"field","maxwell","initial"});
  u.put_region({"scalar","scalar_field","initial"});
  u.put_region({"join","causal_join","genesis"});
  std::string last_join=u.transition("join","universe.genesis",Rational(0),{},"v1",true,[&](const std::string&){
    return std::pair<std::string,InvariantCertificate>{"genesis",pass_cert("genesis","admitted")};
  });

  for(std::size_t step=1;step<=cfg.steps;++step){
    Rational t(static_cast<long long>(step));
    std::vector<std::string> layer;

    CosmologyState next_cos=cos;
    auto ec=u.transition("cosmos","cosmology.friedmann",t,{last_join},cfg.world,true,[&](const std::string&){
      next_cos=friedmann_step(cos,cfg.friedmann_dt);
      return std::pair<std::string,InvariantCertificate>{next_cos.scale_factor.str()+"|"+cfg.world,pass_cert("friedmann","enclosed")};
    });
    cos=std::move(next_cos);
    layer.push_back(ec);

    BodySystem gravity_next=bodies;
    auto eg=u.transition("bodies","mechanics.gravity_kdk",t,{last_join},"G="+cfg.gravity_g.str()+";eps2="+cfg.softening2.str(),true,[&](const std::string&){
      auto r=gravity_kdk(bodies,cfg.gravity_dt,cfg.gravity_g,cfg.softening2);
      gravity_next=r.state;
      return std::pair<std::string,InvariantCertificate>{gravity_next.canonical(),r.invariant};
    });
    bodies=std::move(gravity_next);
    layer.push_back(eg);

    BodySystem decay_next=bodies;
    auto ed=u.transition("bodies","decay.two_body_entropy",t,{eg},"p="+cfg.decay_probability.str(),false,[&](const std::string&){
      auto r=stochastic_two_body_decay(bodies,0,cfg.decay_probability,u.entropy());
      decay_next=r.state;
      return std::pair<std::string,InvariantCertificate>{decay_next.canonical(),r.invariant};
    });
    bodies=std::move(decay_next);
    layer.push_back(ed);

    auto next_ex=ex,next_ey=ey,next_ez=ez,next_bx=bx,next_by=by,next_bz=bz;
    auto ef=u.transition("field","field.maxwell_periodic",t,{last_join},std::to_string(cfg.nx)+"x"+std::to_string(cfg.ny)+"x"+std::to_string(cfg.nz),true,[&](const std::string&){
      auto c=maxwell_step_certified(next_ex,next_ey,next_ez,next_bx,next_by,next_bz,cfg.nx,cfg.ny,cfg.nz);
      InvariantCertificate inv;inv.pass=c.pass;inv.claims={{"divergence",c.pass?"preserved":"violated"},{"before",c.before_hash},{"after",c.after_hash}};
      return std::pair<std::string,InvariantCertificate>{hash_join({c.after_hash,std::to_string(step)}),inv};
    });
    ex=std::move(next_ex);ey=std::move(next_ey);ez=std::move(next_ez);bx=std::move(next_bx);by=std::move(next_by);bz=std::move(next_bz);
    layer.push_back(ef);

    auto es=u.transition("scalar","field.scalar_refinement",t,{last_join},"threshold="+std::to_string(cfg.scalar_threshold),true,[&](const std::string&){
      const bool refine=scalar_needs_refinement(scalar,cfg.nx,cfg.ny,cfg.nz,cfg.scalar_threshold);
      return std::pair<std::string,InvariantCertificate>{refine?"refine":"stable",pass_cert("adaptive_refinement",refine?"split":"hold")};
    });
    layer.push_back(es);

    last_join=u.transition("join","universe.join",t,layer,"step="+std::to_string(step),true,[&](const std::string&){
      return std::pair<std::string,InvariantCertificate>{"joined:"+std::to_string(step),pass_cert("causal_join","admitted")};
    });
  }

  LawProgram p{{{OpCode::LoadX,Rational(0)},{OpCode::PushConst,Rational(0)},{OpCode::Add,Rational(0)},{OpCode::PushConst,Rational(1)},{OpCode::Mul,Rational(0)}}};
  auto rewrite=self_rewrite_kernel(p,{Rational(-2),Rational(0),Rational(3),Rational(7,BigInt(3))});
  auto config=cfg.canonical();
  auto config_root=hex(sha256(config));
  RunReport rep{u.state_root(),u.causal().root_hash(),u.entropy().root(),rewrite.certificate.root(),config_root,u.causal().size(),cfg.world};
  auto report=rep.canonical();
  auto events=events_text(u.causal());
  auto entropy_log=u.entropy().audit_log();
  if(report.size()>kMaxReportBytes||events.size()>kMaxEventsBytes||config.size()>kMaxConfigBytes||entropy_log.size()>kMaxEntropyLogBytes)throw std::length_error("run artifact size bound");
  detail::atomic_write_file(out/"report.txt",report);
  detail::atomic_write_file(out/"events.log",events);
  detail::atomic_write_file(out/"config.txt",config);
  detail::atomic_write_file(out/"entropy.log",entropy_log);
  std::string manifest=hex(sha256(report))+" report.txt\n"+hex(sha256(events))+" events.log\n"+hex(sha256(config))+" config.txt\n"+hex(sha256(entropy_log))+" entropy.log\n";
  manifest+=hex(sha256(manifest))+" manifest.root\n";
  detail::atomic_write_file(out/"MANIFEST.sha256",manifest);
  return rep;
}
RunReport run_reference_universe(std::size_t steps,const std::string&outdir,std::optional<std::uint64_t>seed,const std::string&world){return run_universe(default_reference_config(steps,world),outdir,seed);}
bool verify_reference_run(const std::string&dir,std::string*error){
  try{std::filesystem::path p(dir);if(std::filesystem::is_symlink(std::filesystem::symlink_status(p))||!std::filesystem::is_directory(p))throw std::runtime_error("run path is not a real directory");verify_directory_shape(p);auto ms=detail::read_file_bounded(p/"MANIFEST.sha256",kMaxManifestBytes);auto m=parse_manifest(ms);if(m.size()!=5||!m.contains("report.txt")||!m.contains("events.log")||!m.contains("config.txt")||!m.contains("entropy.log")||!m.contains("manifest.root"))throw std::runtime_error("manifest file set");auto report=detail::read_file_bounded(p/"report.txt",kMaxReportBytes),events=detail::read_file_bounded(p/"events.log",kMaxEventsBytes),config=detail::read_file_bounded(p/"config.txt",kMaxConfigBytes),entropy=detail::read_file_bounded(p/"entropy.log",kMaxEntropyLogBytes);if(hex(sha256(report))!=m["report.txt"]||hex(sha256(events))!=m["events.log"]||hex(sha256(config))!=m["config.txt"]||hex(sha256(entropy))!=m["entropy.log"])throw std::runtime_error("artifact hash mismatch");auto root_line=m["manifest.root"]+" manifest.root\n";auto pos=ms.rfind(root_line);if(pos==std::string::npos||pos+root_line.size()!=ms.size())throw std::runtime_error("manifest root position");auto prefix=ms.substr(0,pos);if(hex(sha256(prefix))!=m["manifest.root"])throw std::runtime_error("manifest root mismatch");if(!report.starts_with(std::string("schema=")+std::string(kRunReportSchema)+"\nversion="+std::string(kVersion)+"\n"))throw std::runtime_error("version/schema mismatch");if(!report.contains("config_root="+hex(sha256(config))+"\n"))throw std::runtime_error("config root mismatch");auto marker=std::string("entropy_root=");auto ep=report.find(marker);if(ep==std::string::npos)throw std::runtime_error("missing entropy root");ep+=marker.size();auto ee=report.find('\n',ep);if(ee==std::string::npos)throw std::runtime_error("entropy root framing");auto expected_entropy=report.substr(ep,ee-ep);if(!lower_hex64(expected_entropy))throw std::runtime_error("entropy root encoding");std::string entropy_error;if(!verify_entropy_log(entropy,expected_entropy,&entropy_error))throw std::runtime_error("entropy log: "+entropy_error);return true;}catch(const std::exception&e){if(error)*error=e.what();return false;}
}
std::string reference_run_manifest_root(const std::string&dir){
  std::string error;
  if(!verify_reference_run(dir,&error))throw std::runtime_error("cannot extract root from unverified run: "+error);
  std::filesystem::path p(dir);
  auto ms=detail::read_file_bounded(p/"MANIFEST.sha256",kMaxManifestBytes);
  auto m=parse_manifest(ms);
  auto it=m.find("manifest.root");
  if(it==m.end())throw std::runtime_error("manifest root missing");
  return it->second;
}
bool verify_reference_run_anchored(const std::string&dir,std::string_view expected,std::string*error){
  try{if(!lower_hex64(expected))throw std::invalid_argument("invalid expected manifest root");std::string inner;if(!verify_reference_run(dir,&inner))throw std::runtime_error(inner);if(reference_run_manifest_root(dir)!=expected)throw std::runtime_error("external manifest anchor mismatch");return true;}catch(const std::exception&e){if(error)*error=e.what();return false;}
}
}  // namespace surreal
