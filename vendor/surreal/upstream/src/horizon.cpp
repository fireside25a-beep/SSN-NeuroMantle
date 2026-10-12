#include "surreal/horizon.hpp"
#include "surreal/hash.hpp"
#include "io_internal.hpp"
#include <sstream>
#include <stdexcept>
#include <vector>

namespace surreal {
namespace {
constexpr std::size_t kMaxHorizonBytes = 64U * 1024U * 1024U;
bool canonical_digest(std::string_view s){try{return hex(unhex256(s))==s;}catch(...){return false;}}
std::string encode_text(std::string_view s){return hex_encode(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()),s.size()));}
std::string decode_text(std::string_view s){auto bytes=hex_decode(s);if(bytes.empty())return {};return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};}
std::string cut(const std::string&s,std::string_view prefix){if(!s.starts_with(prefix))throw std::runtime_error("invalid horizon field");return s.substr(prefix.size());}
}
HorizonRecord seal_horizon(const RegionState&r,const std::string&causal_root,const std::string&path){
  if(r.id.empty()||r.kind.empty())throw std::invalid_argument("horizon region id/kind must be non-empty");
  if(!canonical_digest(causal_root))throw std::invalid_argument("invalid causal root digest");
  constexpr std::string_view magic="SURREAL_HORIZON_V2";
  auto commit=hash_join({magic,r.canonical(),causal_root});
  std::ostringstream o;o<<magic<<"\n"<<"id_hex="<<encode_text(r.id)<<"\n"<<"kind_hex="<<encode_text(r.kind)<<"\n"<<"payload_hex="<<encode_text(r.payload)<<"\n"<<"causal_root="<<causal_root<<"\n"<<"commitment="<<commit<<"\n";
  auto bytes=o.str();if(bytes.size()>kMaxHorizonBytes)throw std::length_error("horizon archive size bound");detail::atomic_write_file(path,bytes);return {r,causal_root,commit};
}
HorizonRecord reopen_horizon(const std::string&path){
  auto data=detail::read_file_bounded(path,kMaxHorizonBytes);std::istringstream in(data);std::vector<std::string> lines;std::string line;while(std::getline(in,line))lines.push_back(line);if(lines.size()!=6)throw std::runtime_error("invalid horizon field count");
  constexpr std::string_view magic="SURREAL_HORIZON_V2";if(lines[0]!=magic)throw std::runtime_error("invalid horizon magic");
  RegionState r{decode_text(cut(lines[1],"id_hex=")),decode_text(cut(lines[2],"kind_hex=")),decode_text(cut(lines[3],"payload_hex="))};if(r.id.empty()||r.kind.empty())throw std::runtime_error("empty horizon region id/kind");
  auto cr=cut(lines[4],"causal_root=");auto cm=cut(lines[5],"commitment=");if(!canonical_digest(cr)||!canonical_digest(cm))throw std::runtime_error("invalid horizon digest encoding");auto expected=hash_join({magic,r.canonical(),cr});if(cm!=expected)throw std::runtime_error("horizon commitment mismatch");return {r,cr,cm};
}
HorizonRecord reopen_horizon_anchored(const std::string&path,std::string_view expected_commitment){
  if(!canonical_digest(expected_commitment))throw std::invalid_argument("invalid expected horizon commitment");
  auto r=reopen_horizon(path);if(r.commitment!=expected_commitment)throw std::runtime_error("external horizon anchor mismatch");return r;
}
}  // namespace surreal
