#include "surreal/admission.hpp"
#include "surreal/causal.hpp"
#include "surreal/qecc.hpp"
#include "surreal/hash.hpp"
#include "surreal/physics.hpp"
#include "surreal/rewrite.hpp"
#include "surreal/units.hpp"
#include "surreal/version.hpp"
#include "canonical_internal.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace surreal {
namespace {
constexpr std::string_view kRequestHeader="SURREAL_ADMISSION_REQUEST_V1";
constexpr std::string_view kReceiptHeader="SURREAL_ADMISSION_RECEIPT_V1";

bool valid_key(std::string_view s){if(s.empty()||s.size()>96)return false;for(unsigned char c:s)if(!(std::islower(c)||std::isdigit(c)||c=='.'||c=='_'))return false;return true;}
bool valid_value(std::string_view s,std::size_t limit=65536){if(s.size()>limit)return false;for(unsigned char c:s)if(c<0x20||c==0x7f)return false;return true;}
std::size_t parse_size(std::string_view s,std::string_view what){std::size_t v=0;auto[p,ec]=std::from_chars(s.data(),s.data()+s.size(),v);if(ec!=std::errc{}||p!=s.data()+s.size())throw std::invalid_argument(std::string("invalid ")+std::string(what));return v;}
unsigned parse_unsigned(std::string_view s,std::string_view what){unsigned v=0;auto[p,ec]=std::from_chars(s.data(),s.data()+s.size(),v);if(ec!=std::errc{}||p!=s.data()+s.size())throw std::invalid_argument(std::string("invalid ")+std::string(what));return v;}
const std::string& need(const AdmissionRequest&r,std::string_view k){auto it=r.fields.find(k);if(it==r.fields.end())throw std::invalid_argument("missing request field: "+std::string(k));return it->second;}
std::string opt(const AdmissionRequest&r,std::string_view k,std::string d={}){auto it=r.fields.find(k);return it==r.fields.end()?std::move(d):it->second;}
void reject_unknown(const AdmissionRequest&r,const std::set<std::string,std::less<>>&allowed){for(const auto&[k,v]:r.fields){(void)v;if(!allowed.contains(k))throw std::invalid_argument("unknown request field: "+k);}}
std::string bytes_hex(std::string_view s){return hex_encode(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()),s.size()));}
std::string from_hex(std::string_view h){auto b=hex_decode(h);return std::string(reinterpret_cast<const char*>(b.data()),b.size());}
std::string receipt_root(const AdmissionReceipt&r){return hash_join({kReceiptHeader,r.status,r.request_id,r.operation,r.request_root,r.result_root,r.request_hex,r.result_hex,r.reason});}
std::string request_wire(const AdmissionRequest&r){std::string s=std::string(kRequestHeader)+"\n";for(const auto&[k,v]:r.fields){s+=k;s+="=";s+=v;s+="\n";}return s;}
std::string result_frame(std::initializer_list<std::pair<std::string_view,std::string>> fields){std::string s="SURREAL_ADMISSION_RESULT_V1\n";for(const auto&[k,v]:fields)detail::append_field(s,k,v);return s;}
AdmissionReceipt make_receipt(const AdmissionRequest&r,std::string status,std::string result,std::string reason){AdmissionReceipt out;out.status=std::move(status);out.request_id=r.request_id();out.operation=r.operation();out.request_root=r.root();out.result_root=hex(sha256(result));out.request_hex=bytes_hex(request_wire(r));out.result_hex=bytes_hex(result);out.reason=std::move(reason);out.receipt_root=receipt_root(out);return out;}
std::set<std::string,std::less<>> base_allowed(){return {"request_id","operation","entropy_policy","max_work"};}
void deterministic_policy(const AdmissionRequest&r){if(opt(r,"entropy_policy","forbid")!="forbid")throw std::invalid_argument("operation requires entropy_policy=forbid");const auto mw=parse_size(opt(r,"max_work","65536"),"max_work");if(mw==0||mw>65536)throw std::invalid_argument("max_work out of range");}
bool known_law(std::string_view law){static const std::set<std::string,std::less<>> ids={"cosmology.friedmann","mechanics.gravity_kdk","electromagnetism.coulomb","radiation.photon_inertial","decay.two_body_entropy","mass.radiation_bookkeeping","field.maxwell_periodic","field.scalar_refinement","formula.physics_atlas"};return ids.contains(law);}
Instruction parse_instruction(std::string_view s){if(s=="LOAD_X")return {OpCode::LoadX,Rational(0)};if(s=="ADD")return {OpCode::Add,Rational(0)};if(s=="MUL")return {OpCode::Mul,Rational(0)};if(s=="DUP")return {OpCode::Dup,Rational(0)};if(s=="SWAP")return {OpCode::Swap,Rational(0)};if(s.starts_with("PUSH:"))return {OpCode::PushConst,Rational::parse(s.substr(5))};throw std::invalid_argument("invalid law instruction");}
AdmissionReceipt execute(const AdmissionRequest&r){
  deterministic_policy(r);const auto op=r.operation();const auto max_work=parse_size(opt(r,"max_work","65536"),"max_work");
  if(op=="qecc.verify"){
    auto allowed=base_allowed();allowed.insert("certificate_hex");reject_unknown(r,allowed);
    const auto encoded=need(r,"certificate_hex");
    if(encoded.size()>2*(80+16*kMaxQeccGenerators))throw std::invalid_argument("qecc certificate byte bound");
    const auto code=parse_qecc_certificate(from_hex(encoded));
    if(code.qubits>max_work||code.generators.size()>max_work)throw std::invalid_argument("qecc structural work bound");
    const auto proof=verify_stabilizer_code(code);
    return make_receipt(r,proof.valid?"ADMIT":"REFUSE",proof.canonical(),proof.valid?"qecc_certificate_valid":"qecc_certificate_invalid");
  }
  if(op=="exact.compare"){
    auto allowed=base_allowed();allowed.insert("lhs");allowed.insert("rhs");allowed.insert("relation");reject_unknown(r,allowed);
    const auto a=Rational::parse(need(r,"lhs")),b=Rational::parse(need(r,"rhs"));const auto rel=need(r,"relation");bool pass=false;if(rel=="eq")pass=a==b;else if(rel=="ne")pass=a!=b;else if(rel=="lt")pass=a<b;else if(rel=="le")pass=a<=b;else if(rel=="gt")pass=a>b;else if(rel=="ge")pass=a>=b;else throw std::invalid_argument("unknown relation");
    auto result=result_frame({{"kind","exact.compare"},{"lhs",a.str()},{"rhs",b.str()},{"relation",rel},{"holds",pass?"1":"0"}});return make_receipt(r,pass?"ADMIT":"REFUSE",result,pass?"relation_holds":"relation_false");
  }
  if(op=="formula.evaluate"){
    std::set<std::string,std::less<>> allowed=base_allowed();allowed.insert("formula");allowed.insert("arg_count");allowed.insert("sqrt_digits");const auto n=parse_size(need(r,"arg_count"),"arg_count");if(n>max_work||n>128)throw std::invalid_argument("formula argument bound");for(std::size_t i=0;i<n;++i){allowed.insert("arg."+std::to_string(i)+".value");allowed.insert("arg."+std::to_string(i)+".unit");}reject_unknown(r,allowed);std::vector<Quantity> args;args.reserve(n);for(std::size_t i=0;i<n;++i)args.push_back(Quantity::from(Rational::parse(need(r,"arg."+std::to_string(i)+".value")),need(r,"arg."+std::to_string(i)+".unit")));const unsigned digits=parse_unsigned(opt(r,"sqrt_digits","36"),"sqrt_digits");if(digits==0||digits>256)throw std::invalid_argument("sqrt_digits out of range");auto v=evaluate_formula(need(r,"formula"),args,digits);auto result=result_frame({{"kind","formula.evaluate"},{"formula",v.formula_id},{"value",v.value.si_value.str()},{"dimension",v.value.dimension.str()}});return make_receipt(r,"ADMIT",result,"formula_evaluated");
  }
  if(op=="causal.validate"){
    std::set<std::string,std::less<>> allowed=base_allowed();allowed.insert("parent_count");allowed.insert("candidate_time");allowed.insert("candidate_region");allowed.insert("candidate_law");const auto n=parse_size(need(r,"parent_count"),"parent_count");if(n>max_work||n>kMaxEventParents)throw std::invalid_argument("causal parent bound");for(std::size_t i=0;i<n;++i){allowed.insert("parent."+std::to_string(i)+".time");allowed.insert("parent."+std::to_string(i)+".region");allowed.insert("parent."+std::to_string(i)+".law");}reject_unknown(r,allowed);const auto cand_law=need(r,"candidate_law");if(!known_law(cand_law))return make_receipt(r,"REFUSE",result_frame({{"kind","causal.validate"},{"admissible","0"}}),"unknown_law");CausalGraph g;std::vector<std::string> parents;parents.reserve(n);for(std::size_t i=0;i<n;++i){const auto plaw=need(r,"parent."+std::to_string(i)+".law");if(!known_law(plaw))return make_receipt(r,"REFUSE",result_frame({{"kind","causal.validate"},{"admissible","0"}}),"unknown_parent_law");Event e;e.local_time=Rational::parse(need(r,"parent."+std::to_string(i)+".time"));e.region=need(r,"parent."+std::to_string(i)+".region");e.law=plaw;e.before_hash=std::string(64,'0');e.after_hash=std::string(64,'0');e.invariant_hash=std::string(64,'0');e.entropy_root=std::string(64,'0');parents.push_back(g.append(std::move(e)));}try{const auto t=Rational::parse(need(r,"candidate_time"));const auto depth=g.validate_admission(parents,t);const auto boundary=causal_boundary_root(g,parents);auto result=result_frame({{"kind","causal.validate"},{"admissible","1"},{"depth",std::to_string(depth)},{"boundary_root",boundary},{"candidate_region",need(r,"candidate_region")},{"candidate_law",cand_law}});return make_receipt(r,"ADMIT",result,"causal_admission_valid");}catch(const std::exception&e){return make_receipt(r,"REFUSE",result_frame({{"kind","causal.validate"},{"admissible","0"}}),std::string("causal_refusal:")+e.what());}
  }
  if(op=="rewrite.verify"){
    std::set<std::string,std::less<>> allowed=base_allowed();allowed.insert("original_count");allowed.insert("claimed_count");allowed.insert("step_count");allowed.insert("certificate.original_hash");allowed.insert("certificate.final_hash");const auto on=parse_size(need(r,"original_count"),"original_count"),cn=parse_size(need(r,"claimed_count"),"claimed_count"),sn=parse_size(need(r,"step_count"),"step_count");if(on>max_work||cn>max_work||sn>max_work||on>kMaxLawInstructions||cn>kMaxLawInstructions||sn>kMaxRewriteSteps)throw std::invalid_argument("rewrite work bound");for(std::size_t i=0;i<on;++i)allowed.insert("original."+std::to_string(i));for(std::size_t i=0;i<cn;++i)allowed.insert("claimed."+std::to_string(i));for(std::size_t i=0;i<sn;++i){auto p="step."+std::to_string(i)+".";allowed.insert(p+"rule");allowed.insert(p+"index");allowed.insert(p+"before");allowed.insert(p+"after");}reject_unknown(r,allowed);LawProgram original,claimed;for(std::size_t i=0;i<on;++i)original.code.push_back(parse_instruction(need(r,"original."+std::to_string(i))));for(std::size_t i=0;i<cn;++i)claimed.code.push_back(parse_instruction(need(r,"claimed."+std::to_string(i))));RewriteCertificate c;c.original_hash=need(r,"certificate.original_hash");c.final_hash=need(r,"certificate.final_hash");for(std::size_t i=0;i<sn;++i){auto p="step."+std::to_string(i)+".";c.steps.push_back({need(r,p+"rule"),parse_size(need(r,p+"index"),"rewrite index"),need(r,p+"before"),need(r,p+"after")});}const bool pass=verify_rewrite_certificate(original,claimed,c);auto result=result_frame({{"kind","rewrite.verify"},{"valid",pass?"1":"0"},{"original_hash",original.hash()},{"claimed_hash",claimed.hash()}});return make_receipt(r,pass?"ADMIT":"REFUSE",result,pass?"rewrite_certificate_valid":"rewrite_certificate_invalid");
  }
  throw std::invalid_argument("unknown admission operation: "+op);
}
}

std::string AdmissionRequest::canonical()const{std::string s=std::string(kRequestHeader)+"\n";for(const auto&[k,v]:fields){detail::append_field(s,k,v);}return s;}
std::string AdmissionRequest::root()const{return hex(sha256(canonical()));}
std::string AdmissionRequest::request_id()const{auto it=fields.find("request_id");if(it==fields.end()||it->second.empty())throw std::invalid_argument("missing request_id");return it->second;}
std::string AdmissionRequest::operation()const{auto it=fields.find("operation");if(it==fields.end()||it->second.empty())throw std::invalid_argument("missing operation");return it->second;}

AdmissionRequest parse_admission_request(std::string_view text){if(text.size()>kMaxAdmissionBytes)throw std::length_error("admission request byte bound");AdmissionRequest r;std::istringstream in{std::string(text)};std::string line;if(!std::getline(in,line)||line!=kRequestHeader)throw std::invalid_argument("invalid admission request schema");while(std::getline(in,line)){if(line.empty())continue;auto p=line.find('=');if(p==std::string::npos)throw std::invalid_argument("invalid admission request line");auto k=line.substr(0,p),v=line.substr(p+1);if(!valid_key(k)||!valid_value(v))throw std::invalid_argument("invalid admission field");if(!r.fields.emplace(std::move(k),std::move(v)).second)throw std::invalid_argument("duplicate admission field");if(r.fields.size()>kMaxAdmissionFields)throw std::length_error("admission field bound");} (void)r.request_id();(void)r.operation();return r;}

std::string AdmissionReceipt::canonical()const{std::ostringstream o;o<<kReceiptHeader<<"\n"<<"status="<<status<<"\n"<<"request_id="<<request_id<<"\n"<<"operation="<<operation<<"\n"<<"request_root="<<request_root<<"\n"<<"result_root="<<result_root<<"\n"<<"request_hex="<<request_hex<<"\n"<<"result_hex="<<result_hex<<"\n"<<"reason="<<reason<<"\n"<<"receipt_root="<<receipt_root<<"\n";return o.str();}
AdmissionReceipt parse_admission_receipt(std::string_view text){if(text.size()>4*kMaxAdmissionBytes)throw std::length_error("admission receipt byte bound");std::istringstream in{std::string(text)};std::string line;if(!std::getline(in,line)||line!=kReceiptHeader)throw std::invalid_argument("invalid admission receipt schema");std::map<std::string,std::string,std::less<>> m;while(std::getline(in,line)){if(line.empty())continue;auto p=line.find('=');if(p==std::string::npos)throw std::invalid_argument("invalid receipt line");auto k=line.substr(0,p),v=line.substr(p+1);if(!valid_key(k)||!valid_value(v,(k=="request_hex"||k=="result_hex")?2*kMaxAdmissionBytes:65536)||!m.emplace(k,v).second)throw std::invalid_argument("invalid receipt field");}const std::set<std::string,std::less<>> keys={"status","request_id","operation","request_root","result_root","request_hex","result_hex","reason","receipt_root"};if(m.size()!=keys.size())throw std::invalid_argument("receipt field count");for(const auto&k:keys)if(!m.contains(k))throw std::invalid_argument("missing receipt field");AdmissionReceipt r{m["status"],m["request_id"],m["operation"],m["request_root"],m["result_root"],m["request_hex"],m["result_hex"],m["reason"],m["receipt_root"]};return r;}
AdmissionReceipt admit(const AdmissionRequest&r){try{return execute(r);}catch(const std::exception&e){return make_receipt(r,"REFUSE",result_frame({{"kind","error"},{"admissible","0"}}),std::string("request_refused:")+e.what());}}
bool verify_admission_receipt(const AdmissionReceipt&r,std::string*error){try{if(r.status!="ADMIT"&&r.status!="REFUSE")throw std::invalid_argument("receipt status");if(receipt_root(r)!=r.receipt_root)throw std::invalid_argument("receipt root mismatch");const auto req_text=from_hex(r.request_hex),result=from_hex(r.result_hex);auto req=parse_admission_request(req_text);if(req.root()!=r.request_root)throw std::invalid_argument("request root mismatch");if(hex(sha256(result))!=r.result_root)throw std::invalid_argument("result root mismatch");auto replay=admit(req);if(replay.status!=r.status||replay.request_id!=r.request_id||replay.operation!=r.operation||replay.request_root!=r.request_root||replay.result_root!=r.result_root||replay.result_hex!=r.result_hex||replay.reason!=r.reason||replay.receipt_root!=r.receipt_root)throw std::invalid_argument("receipt replay mismatch");return true;}catch(const std::exception&e){if(error)*error=e.what();return false;}}
std::string admission_description_json(){return std::string("{\n  \"protocol\":\"")+std::string(kRequestHeader)+"\",\n  \"receipt\":\""+std::string(kReceiptHeader)+"\",\n  \"runtime_version\":\""+std::string(kVersion)+"\",\n  \"transport\":\"local_stdin_or_file_only\",\n  \"network\":false,\n  \"model_dependency\":false,\n  \"operations\":[\"qecc.verify\",\"exact.compare\",\"formula.evaluate\",\"causal.validate\",\"rewrite.verify\"],\n  \"decision\":\"ADMIT_or_REFUSE\",\n  \"receipt_replay_verifiable\":true\n}\n";}
}  // namespace surreal
