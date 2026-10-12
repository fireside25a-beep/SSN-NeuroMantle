#include "test.hpp"
#include "surreal/admission.hpp"
#include "surreal/rewrite.hpp"
#include "surreal/qecc.hpp"
#include "surreal/hash.hpp"
#include <fstream>
#include <filesystem>
#include <string>
using namespace surreal;

static AdmissionRequest req(std::string text){return parse_admission_request(text);}

int main(){
  auto q=req("SURREAL_ADMISSION_REQUEST_V1\nrequest_id=r1\noperation=exact.compare\nentropy_policy=forbid\nlhs=2/4\nrhs=1/2\nrelation=eq\n");
  auto qr=admit(q);CHECK_EQ(qr.status,std::string("ADMIT"));std::string err;CHECK(verify_admission_receipt(qr,&err));
  auto qt=parse_admission_receipt(qr.canonical());CHECK(verify_admission_receipt(qt,&err));
  auto qbad=req("SURREAL_ADMISSION_REQUEST_V1\nrequest_id=r2\noperation=exact.compare\nlhs=1\nrhs=2\nrelation=gt\n");CHECK_EQ(admit(qbad).status,std::string("REFUSE"));

  auto f=req("SURREAL_ADMISSION_REQUEST_V1\nrequest_id=f1\noperation=formula.evaluate\nformula=kinetic_energy\narg_count=2\narg.0.value=2\narg.0.unit=kg\narg.1.value=3\narg.1.unit=m_per_s\n");auto fr=admit(f);CHECK_EQ(fr.status,std::string("ADMIT"));CHECK(verify_admission_receipt(fr,&err));
  auto fb=req("SURREAL_ADMISSION_REQUEST_V1\nrequest_id=f2\noperation=formula.evaluate\nformula=kinetic_energy\narg_count=2\narg.0.value=2\narg.0.unit=m\narg.1.value=3\narg.1.unit=m_per_s\n");CHECK_EQ(admit(fb).status,std::string("REFUSE"));

  auto c=req("SURREAL_ADMISSION_REQUEST_V1\nrequest_id=c1\noperation=causal.validate\nparent_count=1\nparent.0.time=2\nparent.0.region=cosmos\nparent.0.law=cosmology.friedmann\ncandidate_time=3\ncandidate_region=bodies\ncandidate_law=mechanics.gravity_kdk\n");auto cr=admit(c);CHECK_EQ(cr.status,std::string("ADMIT"));CHECK(verify_admission_receipt(cr,&err));
  auto cb=req("SURREAL_ADMISSION_REQUEST_V1\nrequest_id=c2\noperation=causal.validate\nparent_count=1\nparent.0.time=2\nparent.0.region=cosmos\nparent.0.law=cosmology.friedmann\ncandidate_time=1\ncandidate_region=bodies\ncandidate_law=mechanics.gravity_kdk\n");CHECK_EQ(admit(cb).status,std::string("REFUSE"));

  LawProgram lp{{{OpCode::LoadX,Rational(0)},{OpCode::PushConst,Rational(0)},{OpCode::Add,Rational(0)}}};auto forge=self_rewrite_kernel(lp,{Rational(-1),Rational(0),Rational(1)});CHECK(forge.certificate.steps.size()==1);const auto&s=forge.certificate.steps.front();
  std::string rt="SURREAL_ADMISSION_REQUEST_V1\nrequest_id=w1\noperation=rewrite.verify\noriginal_count=3\noriginal.0=LOAD_X\noriginal.1=PUSH:0\noriginal.2=ADD\nclaimed_count=1\nclaimed.0=LOAD_X\nstep_count=1\ncertificate.original_hash="+forge.certificate.original_hash+"\ncertificate.final_hash="+forge.certificate.final_hash+"\nstep.0.rule="+s.rule+"\nstep.0.index="+std::to_string(s.index)+"\nstep.0.before="+s.before_hash+"\nstep.0.after="+s.after_hash+"\n";
  auto wr=admit(req(rt));CHECK_EQ(wr.status,std::string("ADMIT"));CHECK(verify_admission_receipt(wr,&err));
  auto tampered=wr;tampered.result_hex[0]=tampered.result_hex[0]=='0'?'1':'0';CHECK(!verify_admission_receipt(tampered,&err));
  CHECK_THROWS(parse_admission_request("SURREAL_ADMISSION_REQUEST_V1\nrequest_id=x\nrequest_id=y\noperation=exact.compare\n"));
  auto d=admission_description_json();CHECK(d.find("local_stdin_or_file_only")!=std::string::npos);CHECK(d.find("formula.evaluate")!=std::string::npos);
  auto large=q;large.fields["extra"]=std::string(40000,'a');
  auto refused=admit(large);CHECK_EQ(refused.status,std::string("REFUSE"));
  CHECK(verify_admission_receipt(parse_admission_receipt(refused.canonical()),&err));
  const auto proof_dir=test_temp_dir("surreal_admission_unit");
  const auto proof_file=(proof_dir/"five.sqecc").string();
  write_qecc_certificate(five_qubit_perfect_code(),proof_file);
  std::ifstream cert(proof_file,std::ios::binary);
  std::string bytes((std::istreambuf_iterator<char>(cert)),{});
  const auto encode=[](const std::string& b){return hex_encode(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(b.data()),b.size()));};
  auto proof_request=req("SURREAL_ADMISSION_REQUEST_V1\nrequest_id=qecc\noperation=qecc.verify\ncertificate_hex="+encode(bytes)+"\n");
  auto proof_receipt=admit(proof_request);CHECK_EQ(proof_receipt.status,std::string("ADMIT"));
  CHECK(verify_admission_receipt(parse_admission_receipt(proof_receipt.canonical()),&err));
  bytes[80]^=1;proof_request.fields["certificate_hex"]=encode(bytes);
  auto rejected=admit(proof_request);CHECK_EQ(rejected.status,std::string("REFUSE"));CHECK(verify_admission_receipt(rejected,&err));
  proof_request.fields["certificate_hex"]="zz";CHECK_EQ(admit(proof_request).status,std::string("REFUSE"));
  std::filesystem::remove_all(proof_dir);
  return finish_tests();
}
