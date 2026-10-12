#include "surreal/admission.hpp"
#include "surreal/exact.hpp"
#include "surreal/physics.hpp"
#include "surreal/qecc.hpp"
#include "surreal/units.hpp"
#include "surreal/universe.hpp"
#include "surreal/version.hpp"
#include "io_internal.hpp"
#include <array>
#include <charconv>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <optional>
#include <stdexcept>

namespace {
std::string read_stdin_bounded(std::size_t limit){
  std::string text;std::array<char,16384> buffer{};
  while(std::cin){std::cin.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
    const auto got=static_cast<std::size_t>(std::cin.gcount());
    if(got>limit-text.size())throw std::length_error("admission input byte bound");
    text.append(buffer.data(),got);
  }
  if(std::cin.bad())throw std::runtime_error("cannot read admission input");
  return text;
}
std::uint64_t parse_u64(std::string s){int base=10;if(s.starts_with("0x")){base=16;s=s.substr(2);}std::uint64_t v=0;auto[p,ec]=std::from_chars(s.data(),s.data()+s.size(),v,base);if(ec!=std::errc{}||p!=s.data()+s.size())throw std::invalid_argument("invalid uint64");return v;}
std::size_t parse_size(std::string_view s){std::size_t v=0;auto[p,ec]=std::from_chars(s.data(),s.data()+s.size(),v);if(ec!=std::errc{}||p!=s.data()+s.size())throw std::invalid_argument("invalid size");return v;}
}
int main(int argc,char**argv){try{
  if(argc<2){std::cerr<<"SURREAL "<<surreal::kVersion<<"\nusage: surreal demo|run|verify|laws|formulas|math|describe|admit|verify-receipt|qec-prove|qec-verify|qec-describe ...\n";return 2;}
  std::string cmd=argv[1];
  if(cmd=="demo"){if(argc<4||argc>6){std::cerr<<"usage: surreal demo STEPS OUTDIR [SEED] [WORLD]\n";return 2;}auto steps=parse_size(argv[2]);std::optional<std::uint64_t>seed;if(argc>=5)seed=parse_u64(argv[4]);std::string world=argc>=6?argv[5]:"lcdm";auto r=surreal::run_reference_universe(steps,argv[3],seed,world);std::cout<<r.canonical();return 0;}
  if(cmd=="run"){if(argc<4||argc>5){std::cerr<<"usage: surreal run CONFIG OUTDIR [SEED]\n";return 2;}auto cfg=surreal::load_reference_config(argv[2]);std::optional<std::uint64_t>seed;if(argc==5)seed=parse_u64(argv[4]);auto r=surreal::run_universe(cfg,argv[3],seed);std::cout<<r.canonical();return 0;}
  if(cmd=="verify"){if(argc<3||argc>4){std::cerr<<"usage: surreal verify OUTDIR [EXPECTED_MANIFEST_ROOT]\n";return 2;}std::string err;const bool ok=argc==4?surreal::verify_reference_run_anchored(argv[2],argv[3],&err):surreal::verify_reference_run(argv[2],&err);if(!ok){std::cerr<<err<<"\n";return 1;}std::cout<<"verified\n";return 0;}
  if(cmd=="laws"){std::cout<<"[\n";auto v=surreal::law_catalogue_json_lines();for(std::size_t i=0;i<v.size();++i)std::cout<<"  "<<v[i]<<(i+1==v.size()?"\n":",\n");std::cout<<"]\n";return 0;}
  if(cmd=="formulas"){std::cout<<"[\n";auto v=surreal::formula_catalogue_json_lines();for(std::size_t i=0;i<v.size();++i)std::cout<<"  "<<v[i]<<(i+1==v.size()?"\n":",\n");std::cout<<"]\n";return 0;}
  if(cmd=="describe"){std::cout<<surreal::admission_description_json();return 0;}
  if(cmd=="admit"){if(argc>3){std::cerr<<"usage: surreal admit [REQUEST_FILE|-]\n";return 2;}std::string text;if(argc==2||std::string(argv[2])=="-"){text=read_stdin_bounded(surreal::kMaxAdmissionBytes);}else{text=surreal::detail::read_file_bounded(argv[2],surreal::kMaxAdmissionBytes);}auto req=surreal::parse_admission_request(text);auto receipt=surreal::admit(req);std::cout<<receipt.canonical();return receipt.status=="ADMIT"?0:3;}
  if(cmd=="verify-receipt"){if(argc!=3){std::cerr<<"usage: surreal verify-receipt RECEIPT_FILE\n";return 2;}auto receipt=surreal::parse_admission_receipt(surreal::detail::read_file_bounded(argv[2],4*surreal::kMaxAdmissionBytes));std::string err;if(!surreal::verify_admission_receipt(receipt,&err)){std::cerr<<err<<"\n";return 1;}std::cout<<"verified\n";return 0;}
  if(cmd=="qec-prove"){if(argc!=3){std::cerr<<"usage: surreal qec-prove OUTDIR\n";return 2;}std::filesystem::create_directories(argv[2]);auto code=surreal::five_qubit_perfect_code();auto report=surreal::verify_stabilizer_code(code);if(!report.valid)throw std::runtime_error(report.reason);auto cert=(std::filesystem::path(argv[2])/"five_qubit.sqecc").string();surreal::write_qecc_certificate(code,cert);std::ofstream proof(std::filesystem::path(argv[2])/"proof.txt",std::ios::binary|std::ios::trunc);if(!proof)throw std::runtime_error("cannot write qecc proof report");proof<<report.canonical();proof.close();std::cout<<report.canonical();return 0;}
  if(cmd=="qec-verify"){if(argc!=3){std::cerr<<"usage: surreal qec-verify CERTIFICATE\n";return 2;}surreal::QeccProofReport report;std::string err;if(!surreal::verify_qecc_certificate(argv[2],&report,&err)){std::cerr<<err<<"\n";return 1;}std::cout<<report.canonical();return 0;}
  if(cmd=="qec-describe"){if(argc!=2){std::cerr<<"usage: surreal qec-describe\n";return 2;}std::cout<<surreal::qecc_description_json();return 0;}
  if(cmd=="math"){using namespace surreal;Quadratic phi(Rational(1,BigInt(2)),Rational(1,BigInt(2)),BigInt(5));auto relation=phi*phi-phi-Quadratic(Rational(1),Rational(0),BigInt(5));auto h=HahnSeries::omega()*HahnSeries::infinitesimal();std::cout<<"phi="<<phi.str()<<"\nphi^2-phi-1="<<relation.str()<<"\nomega*(1/omega)="<<h.str()<<"\n";return 0;}
  std::cerr<<"unknown command\n";return 2;
}catch(const std::exception&e){std::cerr<<"error: "<<e.what()<<"\n";return 1;}}
