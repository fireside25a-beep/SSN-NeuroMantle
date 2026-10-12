#include "test.hpp"
#include "surreal/horizon.hpp"
#include "surreal/universe.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace {
std::string slurp(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
void spit(const std::filesystem::path&p,const std::string&s){std::ofstream out(p,std::ios::binary|std::ios::trunc);out.write(s.data(),static_cast<std::streamsize>(s.size()));}
}
int main(){using namespace surreal;
  CHECK_THROWS(Interval(Rational(2),Rational(1)));CHECK_THROWS(Interval(Rational(1))/Interval(Rational(-1),Rational(1)));CHECK_THROWS(sqrt_interval(Interval(Rational(-1))));
  auto base=test_temp_dir("surreal_failure_lab_");std::filesystem::remove_all(base);std::filesystem::create_directories(base);

  RegionState r{"r","demo","secret"};seal_horizon(r,std::string(64,'0'),(base/"h.dat").string());{auto all=slurp(base/"h.dat");auto p=all.find("736563726574");CHECK(p!=std::string::npos);if(p!=std::string::npos)all[p]=(all[p]=='7'?'6':'7');spit(base/"h.dat",all);}CHECK_THROWS(reopen_horizon((base/"h.dat").string()));
  spit(base/"truncated.h","SURREAL_HORIZON_V2\nid_hex=72\n");CHECK_THROWS(reopen_horizon((base/"truncated.h").string()));

  auto run1=(base/"run1").string(),run2=(base/"run2").string();auto a=run_reference_universe(3,run1,0x1234ULL,"lcdm");auto b=run_reference_universe(3,run2,0x1234ULL,"lcdm");CHECK_EQ(a.canonical(),b.canonical());CHECK_EQ(slurp(std::filesystem::path(run1)/"events.log"),slurp(std::filesystem::path(run2)/"events.log"));CHECK_EQ(slurp(std::filesystem::path(run1)/"entropy.log"),slurp(std::filesystem::path(run2)/"entropy.log"));std::string err;CHECK(verify_reference_run(run1,&err));auto manifest_anchor=reference_run_manifest_root(run1);CHECK(verify_reference_run_anchored(run1,manifest_anchor,&err));CHECK(!verify_reference_run_anchored(run1,std::string(64,'0'),&err));
  {auto path=std::filesystem::path(run1)/"events.log";auto all=slurp(path);CHECK(!all.empty());if(!all.empty())all[0]=(all[0]=='X'?'Y':'X');spit(path,all);}CHECK(!verify_reference_run(run1,&err));

  auto extra=base/"extra";run_reference_universe(1,extra.string(),1,"lcdm");spit(extra/"unexpected.txt","x");CHECK(!verify_reference_run(extra.string(),&err));
  auto symlink_run=base/"symlink-run";run_reference_universe(1,symlink_run.string(),11,"lcdm");auto outside=base/"outside-report.txt";spit(outside,slurp(symlink_run/"report.txt"));std::filesystem::remove(symlink_run/"report.txt");std::filesystem::create_symlink(outside,symlink_run/"report.txt");CHECK(!verify_reference_run(symlink_run.string(),&err));
  auto real_out=base/"real-out";std::filesystem::create_directories(real_out);auto linked_out=base/"linked-out";std::filesystem::create_directory_symlink(real_out,linked_out);CHECK_THROWS(run_reference_universe(1,linked_out.string(),12,"lcdm"));
  auto badmanifest=base/"badmanifest";run_reference_universe(1,badmanifest.string(),2,"lcdm");auto manifest=slurp(badmanifest/"MANIFEST.sha256");auto sp=manifest.find(' ');CHECK(sp!=std::string::npos);if(sp!=std::string::npos)manifest.replace(0,sp,"xyz");spit(badmanifest/"MANIFEST.sha256",manifest);CHECK(!verify_reference_run(badmanifest.string(),&err));
  auto traversal=base/"traversal";run_reference_universe(1,traversal.string(),3,"lcdm");manifest=slurp(traversal/"MANIFEST.sha256");auto needle=std::string(" report.txt");auto pos=manifest.find(needle);CHECK(pos!=std::string::npos);if(pos!=std::string::npos)manifest.replace(pos,needle.size()," ../report.txt");spit(traversal/"MANIFEST.sha256",manifest);CHECK(!verify_reference_run(traversal.string(),&err));

  auto cfg=default_reference_config(2,"eds");auto cfgpath=base/"config.txt";spit(cfgpath,cfg.canonical());auto parsed=load_reference_config(cfgpath.string());CHECK_EQ(parsed.canonical(),cfg.canonical());auto custom=run_universe(parsed,(base/"custom").string(),123);CHECK_EQ(custom.world,std::string("eds"));CHECK(verify_reference_run((base/"custom").string(),&err));
  {std::ofstream o(base/"badcfg");o<<"SURREAL_REFERENCE_CONFIG_V1\nsteps=1\nworld=lcdm\nunknown=x\n";}CHECK_THROWS(load_reference_config((base/"badcfg").string()));
  {auto text=cfg.canonical();auto q=text.find("steps=2");CHECK(q!=std::string::npos);if(q!=std::string::npos)text.replace(q,7,"steps=2junk");spit(base/"junk-steps.cfg",text);}CHECK_THROWS(load_reference_config((base/"junk-steps.cfg").string()));
  {auto text=cfg.canonical();auto q=text.find("field_dims=4,4,4");CHECK(q!=std::string::npos);if(q!=std::string::npos)text.replace(q,16,"field_dims=4x,4,4");spit(base/"junk-dims.cfg",text);}CHECK_THROWS(load_reference_config((base/"junk-dims.cfg").string()));
  auto ranged=cfg;ranged.bodies.bodies[0].position.x=Interval(Rational(-2),Rational(-1));CHECK_THROWS(ranged.validate());
  auto reordered=cfg;if(reordered.ex.size()<2){reordered.ex.push_back({3,9});reordered.ex.push_back({2,8});}auto reordered2=reordered;std::reverse(reordered2.ex.begin(),reordered2.ex.end());CHECK_EQ(reordered.canonical(),reordered2.canonical());
  CHECK_THROWS(run_reference_universe(0,(base/"bad").string(),1,"lcdm"));CHECK_THROWS(run_reference_universe(1,(base/"badworld").string(),1,"unknown"));CHECK_THROWS(run_reference_universe(1,(base/"custom").string(),1,"lcdm"));

  std::filesystem::remove_all(base);return finish_tests();}
