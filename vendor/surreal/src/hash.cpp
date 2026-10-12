#include "surreal/hash.hpp"
#include <array>
#include <charconv>
#include <cstring>
#include <stdexcept>

namespace surreal {
namespace {
constexpr std::array<std::uint32_t,64> K = {
  0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
  0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
  0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
  0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
  0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
  0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
  0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
  0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
constexpr std::array<std::uint32_t,8> IV = {0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
constexpr std::uint32_t rotr(std::uint32_t x, int n){ return (x>>n)|(x<<(32-n)); }
void compress(std::array<std::uint32_t,8>& h, const std::uint8_t* block){
  std::uint32_t w[64]{};
  for(int i=0;i<16;++i) w[i]=(std::uint32_t(block[4*i])<<24)|(std::uint32_t(block[4*i+1])<<16)|(std::uint32_t(block[4*i+2])<<8)|block[4*i+3];
  for(int i=16;i<64;++i){ auto s0=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3); auto s1=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10); w[i]=w[i-16]+s0+w[i-7]+s1; }
  auto [a,b,c,d,e,f,g,hh]=h;
  for(int i=0;i<64;++i){ auto S1=rotr(e,6)^rotr(e,11)^rotr(e,25); auto ch=(e&f)^((~e)&g); auto t1=hh+S1+ch+K[i]+w[i]; auto S0=rotr(a,2)^rotr(a,13)^rotr(a,22); auto maj=(a&b)^(a&c)^(b&c); auto t2=S0+maj; hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; }
  h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
}
int nibble(char c){ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; throw std::invalid_argument("invalid hex"); }
}
Digest256 sha256(std::span<const std::uint8_t> bytes){
  std::array<std::uint32_t,8> h=IV; std::size_t full=bytes.size()/64;
  for(std::size_t i=0;i<full;++i) compress(h, bytes.data()+i*64);
  std::array<std::uint8_t,128> tail{}; std::size_t rem=bytes.size()%64; if(rem) std::memcpy(tail.data(),bytes.data()+full*64,rem); tail[rem]=0x80;
  std::size_t pad_blocks=(rem<56)?1:2; std::uint64_t bits=static_cast<std::uint64_t>(bytes.size())*8U;
  for(int i=0;i<8;++i) tail[pad_blocks*64-1-i]=static_cast<std::uint8_t>(bits>>(8*i));
  for(std::size_t i=0;i<pad_blocks;++i) compress(h,tail.data()+i*64);
  Digest256 out{}; for(int i=0;i<8;++i){out[4*i]=h[i]>>24;out[4*i+1]=h[i]>>16;out[4*i+2]=h[i]>>8;out[4*i+3]=h[i];} return out;
}
Digest256 sha256(std::string_view text){ return sha256(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()),text.size())); }
std::string hex(const Digest256& d){ static constexpr char H[]="0123456789abcdef"; std::string s; s.resize(64); for(std::size_t i=0;i<32;++i){s[2*i]=H[d[i]>>4];s[2*i+1]=H[d[i]&15];} return s; }
Digest256 unhex256(std::string_view s){ if(s.size()!=64) throw std::invalid_argument("sha256 hex length"); Digest256 d{}; for(std::size_t i=0;i<32;++i)d[i]=static_cast<std::uint8_t>((nibble(s[2*i])<<4)|nibble(s[2*i+1])); return d; }
std::string hash_join(std::initializer_list<std::string_view> parts){ std::string x; for(auto p:parts){x+=std::to_string(p.size());x+=':';x.append(p);x+='|';} return hex(sha256(x)); }
std::string hex_encode(std::span<const std::uint8_t> bytes){ static constexpr char H[]="0123456789abcdef"; std::string s(bytes.size()*2,'0'); for(std::size_t i=0;i<bytes.size();++i){s[2*i]=H[bytes[i]>>4];s[2*i+1]=H[bytes[i]&15];} return s; }
std::vector<std::uint8_t> hex_decode(std::string_view s){ if(s.size()%2) throw std::invalid_argument("odd hex length"); std::vector<std::uint8_t> out(s.size()/2); for(std::size_t i=0;i<out.size();++i) out[i]=static_cast<std::uint8_t>((nibble(s[2*i])<<4)|nibble(s[2*i+1])); return out; }
}
