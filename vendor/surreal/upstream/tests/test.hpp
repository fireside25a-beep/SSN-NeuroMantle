#pragma once
#include <exception>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

inline int surreal_failures=0;
#define CHECK(expr) do{if(!(expr)){std::cerr<<__FILE__<<":"<<__LINE__<<": CHECK failed: " #expr "\n";++surreal_failures;}}while(0)
#define CHECK_EQ(a,b) do{auto _a=(a);auto _b=(b);if(!(_a==_b)){std::cerr<<__FILE__<<":"<<__LINE__<<": CHECK_EQ failed: " #a " vs " #b "\n";++surreal_failures;}}while(0)
#define CHECK_THROWS(expr) do{bool _ok=false;try{(void)(expr);}catch(...){_ok=true;}if(!_ok){std::cerr<<__FILE__<<":"<<__LINE__<<": CHECK_THROWS failed: " #expr "\n";++surreal_failures;}}while(0)
inline int finish_tests(){if(surreal_failures){std::cerr<<surreal_failures<<" failure(s)\n";return 1;}std::cout<<"PASS\n";return 0;}

inline std::filesystem::path test_temp_dir(const std::string& prefix){
  auto pattern=(std::filesystem::temp_directory_path()/(prefix+".XXXXXX")).string();
  if(!::mkdtemp(pattern.data()))throw std::runtime_error("cannot create isolated test directory");
  return pattern;
}
