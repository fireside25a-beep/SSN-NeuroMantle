#include "surreal/qecc.hpp"
#include "surreal/hash.hpp"
#include "canonical_internal.hpp"
#include "io_internal.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace surreal {
namespace {
constexpr std::size_t kHeaderBytes = 80;
constexpr std::uint64_t kCertificateVersion = 1;

std::uint64_t valid_mask(std::size_t n) {
  if (n == 0 || n > kMaxQeccQubits) throw std::invalid_argument("qecc qubit bound");
  return (std::uint64_t{1} << n) - 1;
}
void validate_mask(PauliMask p, std::uint64_t mask, const char* name) {
  if (((p.x | p.z) & ~mask) != 0) throw std::invalid_argument(std::string(name)+" uses out-of-range qubit");
}
PauliMask pxor(PauliMask a, PauliMask b) noexcept { return {a.x ^ b.x, a.z ^ b.z}; }

std::size_t gf2_rank(std::vector<PauliMask> rows, std::size_t n) {
  std::size_t rank = 0;
  for (std::size_t col = 0; col < 2*n && rank < rows.size(); ++col) {
    const bool xcol = col < n;
    const std::size_t bit = xcol ? col : col-n;
    const std::uint64_t b = std::uint64_t{1} << bit;
    std::size_t pivot = rank;
    while (pivot < rows.size() && (((xcol ? rows[pivot].x : rows[pivot].z) & b) == 0)) ++pivot;
    if (pivot == rows.size()) continue;
    std::swap(rows[rank], rows[pivot]);
    for (std::size_t r = 0; r < rows.size(); ++r) {
      if (r != rank && ((xcol ? rows[r].x : rows[r].z) & b) != 0) rows[r] = pxor(rows[r], rows[rank]);
    }
    ++rank;
  }
  return rank;
}

bool in_stabilizer_span(const StabilizerCode& code, PauliMask p, std::size_t rank) {
  auto rows = code.generators;
  rows.push_back(p);
  return gf2_rank(std::move(rows), code.qubits) == rank;
}

bool commutes_with_all(const StabilizerCode& code, PauliMask p) {
  return std::all_of(code.generators.begin(), code.generators.end(), [&](const PauliMask& g){return pauli_commutes(p,g);});
}

PauliMask single_pauli(std::size_t q, unsigned kind) {
  const auto b = std::uint64_t{1} << q;
  if (kind == 1) return {b,0};
  if (kind == 2) return {0,b};
  return {b,b};
}

void append_u64(std::string& out, std::uint64_t v) {
  for (unsigned i=0;i<8;++i) out.push_back(static_cast<char>((v>>(8*i))&0xffU));
}
std::uint64_t read_u64(std::string_view s, std::size_t off) {
  if (off+8>s.size()) throw std::invalid_argument("truncated qecc certificate");
  std::uint64_t v=0;
  for(unsigned i=0;i<8;++i) v|=std::uint64_t(static_cast<unsigned char>(s[off+i]))<<(8*i);
  return v;
}

PauliMask parse_literal(std::string_view s) {
  if (s.empty() || s.size() > kMaxQeccQubits) throw std::invalid_argument("invalid Pauli literal length");
  PauliMask p;
  for (std::size_t i=0;i<s.size();++i) {
    const auto b=std::uint64_t{1}<<i;
    switch(s[i]) {
      case 'I': break;
      case 'X': p.x|=b; break;
      case 'Z': p.z|=b; break;
      case 'Y': p.x|=b; p.z|=b; break;
      default: throw std::invalid_argument("invalid Pauli literal");
    }
  }
  return p;
}

std::string code_canonical(const StabilizerCode& code) {
  std::string s="SURREAL_QECC_CODE_V1\n";
  detail::append_field(s,"qubits",std::to_string(code.qubits));
  detail::append_field(s,"generator_count",std::to_string(code.generators.size()));
  for(std::size_t i=0;i<code.generators.size();++i){
    detail::append_field(s,"g."+std::to_string(i)+".x",std::to_string(code.generators[i].x));
    detail::append_field(s,"g."+std::to_string(i)+".z",std::to_string(code.generators[i].z));
  }
  detail::append_field(s,"logical_x.x",std::to_string(code.logical_x.x));
  detail::append_field(s,"logical_x.z",std::to_string(code.logical_x.z));
  detail::append_field(s,"logical_z.x",std::to_string(code.logical_z.x));
  detail::append_field(s,"logical_z.z",std::to_string(code.logical_z.z));
  detail::append_field(s,"distance_witness.x",std::to_string(code.distance_witness.x));
  detail::append_field(s,"distance_witness.z",std::to_string(code.distance_witness.z));
  return s;
}
} // namespace

bool pauli_commutes(const PauliMask& a, const PauliMask& b) noexcept {
  return (std::popcount((a.x & b.z) ^ (a.z & b.x)) & 1) == 0;
}
std::size_t pauli_weight(const PauliMask& p) noexcept { return std::popcount(p.x | p.z); }
std::uint64_t stabilizer_syndrome(const StabilizerCode& code, const PauliMask& error) {
  if (code.generators.size() > 63) throw std::invalid_argument("syndrome bit bound");
  std::uint64_t syndrome=0;
  for(std::size_t i=0;i<code.generators.size();++i) if(!pauli_commutes(error,code.generators[i])) syndrome|=std::uint64_t{1}<<i;
  return syndrome;
}
std::string qecc_code_root(const StabilizerCode& code){return hex(sha256(code_canonical(code)));}

std::string QeccProofReport::canonical() const {
  std::string s="SURREAL_QECC_PROOF_V1\n";
  detail::append_field(s,"valid",valid?"1":"0");
  detail::append_field(s,"qubits",std::to_string(qubits));
  detail::append_field(s,"stabilizer_rank",std::to_string(stabilizer_rank));
  detail::append_field(s,"logical_qubits",std::to_string(logical_qubits));
  detail::append_field(s,"distance_lower_bound",std::to_string(distance_lower_bound));
  detail::append_field(s,"distance_upper_bound",std::to_string(distance_upper_bound));
  detail::append_field(s,"distinct_single_error_syndromes",std::to_string(distinct_single_error_syndromes));
  detail::append_field(s,"code_root",code_root);
  detail::append_field(s,"reason",reason);
  return s;
}

QeccProofReport verify_stabilizer_code(const StabilizerCode& code) {
  QeccProofReport r; r.qubits=code.qubits;
  try {
    const auto mask=valid_mask(code.qubits);
    if(code.generators.empty()||code.generators.size()>kMaxQeccGenerators) throw std::invalid_argument("qecc generator bound");
    if(code.generators.size()>=code.qubits) throw std::invalid_argument("code must encode at least one logical qubit");
    for(const auto&g:code.generators){validate_mask(g,mask,"generator");if((g.x|g.z)==0)throw std::invalid_argument("identity stabilizer generator");}
    validate_mask(code.logical_x,mask,"logical_x"); validate_mask(code.logical_z,mask,"logical_z"); validate_mask(code.distance_witness,mask,"distance_witness");
    for(std::size_t i=0;i<code.generators.size();++i)for(std::size_t j=i+1;j<code.generators.size();++j)if(!pauli_commutes(code.generators[i],code.generators[j]))throw std::invalid_argument("noncommuting stabilizer generators");
    const auto rank=gf2_rank(code.generators,code.qubits); r.stabilizer_rank=rank; r.logical_qubits=code.qubits-rank;
    if(rank!=code.generators.size())throw std::invalid_argument("dependent stabilizer generator");
    if(r.logical_qubits!=1)throw std::invalid_argument("proof profile currently requires one logical qubit");
    if(!commutes_with_all(code,code.logical_x)||!commutes_with_all(code,code.logical_z))throw std::invalid_argument("logical operator outside normalizer");
    if(pauli_commutes(code.logical_x,code.logical_z))throw std::invalid_argument("logical X/Z must anticommute");
    if(in_stabilizer_span(code,code.logical_x,rank)||in_stabilizer_span(code,code.logical_z,rank))throw std::invalid_argument("logical operator is stabilizer");

    std::set<std::uint64_t> syndromes;
    for(std::size_t q=0;q<code.qubits;++q)for(unsigned kind=1;kind<=3;++kind){const auto s=stabilizer_syndrome(code,single_pauli(q,kind));if(s==0)throw std::invalid_argument("undetected weight-1 Pauli error");if(!syndromes.insert(s).second)throw std::invalid_argument("single-qubit Pauli syndrome collision in nondegenerate proof profile");}
    r.distinct_single_error_syndromes=syndromes.size();

    r.distance_lower_bound=3;
    for(std::size_t q=0;q<code.qubits;++q)for(unsigned kind=1;kind<=3;++kind){const auto p=single_pauli(q,kind);if(commutes_with_all(code,p)&&!in_stabilizer_span(code,p,rank))throw std::invalid_argument("distance below 3: weight-1 logical operator");}
    for(std::size_t a=0;a<code.qubits;++a)for(std::size_t b=a+1;b<code.qubits;++b)for(unsigned ka=1;ka<=3;++ka)for(unsigned kb=1;kb<=3;++kb){const auto p=pxor(single_pauli(a,ka),single_pauli(b,kb));if(commutes_with_all(code,p)&&!in_stabilizer_span(code,p,rank))throw std::invalid_argument("distance below 3: weight-2 logical operator");}
    if(pauli_weight(code.distance_witness)!=3||!commutes_with_all(code,code.distance_witness)||in_stabilizer_span(code,code.distance_witness,rank))throw std::invalid_argument("invalid weight-3 distance witness");
    r.distance_upper_bound=3;
    r.code_root=qecc_code_root(code); r.valid=true; r.reason="certified_[[n,1,3]]_nondegenerate_single_qubit_Pauli_correction";
  } catch(const std::exception& e) { r.valid=false; r.reason=e.what(); if(r.code_root.empty()){try{r.code_root=qecc_code_root(code);}catch(...){}} }
  return r;
}

StabilizerCode five_qubit_perfect_code() {
  StabilizerCode c; c.qubits=5;
  c.generators={parse_literal("XZZXI"),parse_literal("IXZZX"),parse_literal("XIXZZ"),parse_literal("ZXIXZ")};
  c.logical_x=parse_literal("XXXXX"); c.logical_z=parse_literal("ZZZZZ"); c.distance_witness=parse_literal("XYXII");
  return c;
}

void write_qecc_certificate(const StabilizerCode& code,const std::string& path){
  const auto report=verify_stabilizer_code(code); if(!report.valid)throw std::invalid_argument("refusing to serialize invalid qecc code: "+report.reason);
  std::string out; out.reserve(kHeaderBytes+16*code.generators.size()); out.append(kQeccCertificateMagic);
  append_u64(out,kCertificateVersion); append_u64(out,code.qubits); append_u64(out,code.generators.size());
  append_u64(out,code.logical_x.x); append_u64(out,code.logical_x.z); append_u64(out,code.logical_z.x); append_u64(out,code.logical_z.z); append_u64(out,code.distance_witness.x); append_u64(out,code.distance_witness.z);
  for(const auto&g:code.generators){append_u64(out,g.x);append_u64(out,g.z);} detail::atomic_write_file(path,out);
}

StabilizerCode parse_qecc_certificate(std::string_view data){
  if(data.size()<kHeaderBytes||data.substr(0,8)!=kQeccCertificateMagic)throw std::invalid_argument("invalid qecc certificate magic");
  if(read_u64(data,8)!=kCertificateVersion)throw std::invalid_argument("unsupported qecc certificate version");
  StabilizerCode c; c.qubits=read_u64(data,16); const auto m=read_u64(data,24);
  if(m>kMaxQeccGenerators||data.size()!=kHeaderBytes+16*m)throw std::invalid_argument("invalid qecc certificate size");
  c.logical_x={read_u64(data,32),read_u64(data,40)}; c.logical_z={read_u64(data,48),read_u64(data,56)}; c.distance_witness={read_u64(data,64),read_u64(data,72)};
  c.generators.reserve(m); for(std::size_t i=0;i<m;++i)c.generators.push_back({read_u64(data,kHeaderBytes+16*i),read_u64(data,kHeaderBytes+16*i+8)});
  return c;
}

StabilizerCode read_qecc_certificate(const std::string& path){
  return parse_qecc_certificate(detail::read_file_bounded(path,kHeaderBytes+16*kMaxQeccGenerators));
}

bool verify_qecc_certificate(const std::string& path,QeccProofReport*report,std::string*error){try{auto r=verify_stabilizer_code(read_qecc_certificate(path));if(report)*report=r;if(!r.valid){if(error)*error=r.reason;return false;}return true;}catch(const std::exception&e){if(error)*error=e.what();if(report){report->valid=false;report->reason=e.what();}return false;}}

std::string qecc_description_json(){return "{\n  \"certificate\":\"SQECC001\",\n  \"algebra\":\"binary_symplectic_stabilizer\",\n  \"proof_profile\":\"[[n,1,3]] nondegenerate stabilizer code\",\n  \"checks\":[\"generator_bounds\",\"pairwise_commutation\",\"GF2_independence\",\"logical_normalizer\",\"logical_anticommutation\",\"single_error_syndromes\",\"weight_1_2_distance_lower_bound\",\"weight_3_logical_witness\"],\n  \"claim_scope\":\"finite_qecc_statement_only\"\n}\n";}
} // namespace surreal
