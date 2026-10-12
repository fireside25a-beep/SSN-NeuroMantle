#include "test.hpp"
#include "surreal/qecc.hpp"
#include <filesystem>
#include <fstream>
#include <string>

int main(){
  using namespace surreal;
  const auto code=five_qubit_perfect_code();
  const auto report=verify_stabilizer_code(code);
  CHECK(report.valid);
  CHECK_EQ(report.qubits,5U);
  CHECK_EQ(report.stabilizer_rank,4U);
  CHECK_EQ(report.logical_qubits,1U);
  CHECK_EQ(report.distance_lower_bound,3U);
  CHECK_EQ(report.distance_upper_bound,3U);
  CHECK_EQ(report.distinct_single_error_syndromes,15U);
  CHECK(!report.code_root.empty());

  const auto dir=test_temp_dir("surreal_qecc_unit");
  std::filesystem::create_directories(dir);
  const auto cert=(dir/"five.sqecc").string();
  write_qecc_certificate(code,cert);
  QeccProofReport reread; std::string err;
  CHECK(verify_qecc_certificate(cert,&reread,&err));
  CHECK_EQ(reread.code_root,report.code_root);

  auto bad=code;
  bad.generators[1]=bad.generators[0];
  CHECK(!verify_stabilizer_code(bad).valid);
  bad=code;
  bad.logical_z=bad.logical_x;
  CHECK(!verify_stabilizer_code(bad).valid);
  bad=code;
  bad.distance_witness={1,0};
  CHECK(!verify_stabilizer_code(bad).valid);

  std::ifstream in(cert,std::ios::binary); std::string bytes((std::istreambuf_iterator<char>(in)),{});
  bytes[80]^=1;
  const auto tampered=(dir/"tampered.sqecc").string(); std::ofstream out(tampered,std::ios::binary|std::ios::trunc);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.close();
  CHECK(!verify_qecc_certificate(tampered,nullptr,&err));
  std::filesystem::remove_all(dir);
  return finish_tests();
}
