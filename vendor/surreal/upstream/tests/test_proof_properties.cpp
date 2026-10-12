#include "test.hpp"
#include "surreal/qecc.hpp"
#include "surreal/exact.hpp"
#include "surreal/hash.hpp"
#include <array>
#include <set>
#include <algorithm>
#include <iostream>

int main(){
  using namespace surreal;
  const auto code=five_qubit_perfect_code();
  std::set<unsigned> span;
  for(unsigned s=0;s<16;++s){unsigned x=0,z=0;for(unsigned i=0;i<4;++i)if(s&(1U<<i)){x^=code.generators[i].x;z^=code.generators[i].z;}span.insert(x|(z<<5));}
  CHECK_EQ(span.size(),16U);
  unsigned normalizer=0,min_weight=6,correctable=0;
  std::array<unsigned,16> hist{};
  for(unsigned x=0;x<32;++x)for(unsigned z=0;z<32;++z){
    PauliMask p{x,z};auto syndrome=stabilizer_syndrome(code,p);CHECK(syndrome<16);if(syndrome<16)++hist[syndrome];
    // Independent bit-by-bit symplectic oracle, without popcount.
    unsigned reference=0;
    for(unsigned g=0;g<4;++g){unsigned parity=0;for(unsigned q=0;q<5;++q)parity^=((x>>q)&1U)*((code.generators[g].z>>q)&1U)^((z>>q)&1U)*((code.generators[g].x>>q)&1U);reference|=parity<<g;}
    CHECK_EQ(syndrome,reference);
    if(syndrome==0){++normalizer;if(!span.contains(x|(z<<5)))min_weight=std::min(min_weight,static_cast<unsigned>(pauli_weight(p)));}
    if(pauli_weight(p)<=1){PauliMask correction{};if(syndrome)for(unsigned a=0;a<32;++a)for(unsigned b=0;b<32;++b)if(pauli_weight({a,b})==1&&stabilizer_syndrome(code,{a,b})==syndrome)correction={a,b};CHECK(span.contains(static_cast<unsigned>((x^correction.x)|((z^correction.z)<<5))));++correctable;}
  }
  CHECK_EQ(normalizer,64U);CHECK_EQ(min_weight,3U);CHECK_EQ(correctable,16U);for(auto n:hist)CHECK_EQ(n,64U);
  // Enumerate every ordered independent basis of GF(2)^4: |GL(4,2)|=20160.
  unsigned bases=0;
  for(unsigned a=1;a<16;++a)for(unsigned b=1;b<16;++b)for(unsigned c=1;c<16;++c)for(unsigned d=1;d<16;++d){
    std::set<unsigned> combinations;for(unsigned t=0;t<16;++t)combinations.insert(((t&1)?a:0)^((t&2)?b:0)^((t&4)?c:0)^((t&8)?d:0));if(combinations.size()!=16)continue;
    auto transformed=code;std::array<unsigned,4> rows{a,b,c,d};for(unsigned i=0;i<4;++i){transformed.generators[i]={};for(unsigned j=0;j<4;++j)if(rows[i]&(1U<<j)){transformed.generators[i].x^=code.generators[j].x;transformed.generators[i].z^=code.generators[j].z;}}
    CHECK(verify_stabilizer_code(transformed).valid);++bases;
  }
  CHECK_EQ(bases,20160U);
  for(int i=-100;i<100;++i)for(int j=1;j<=50;++j){Rational a(i,BigInt(j)),b(j,BigInt(101));CHECK_EQ((a+b)-b,a);CHECK_EQ(a/b*b,a);CHECK_EQ(Rational::parse(a.str()),a);}
  CHECK_EQ(hex(sha256("")),std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  CHECK_EQ(hex(sha256(std::string(1000000,'a'))),std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
  std::cout<<"exhaustive_paulis=1024 independent_bases="<<bases<<" rational_inputs=10000\n";
  return finish_tests();
}
