#include "surreal/causal.hpp"
#include "surreal/hash.hpp"
#include "canonical_internal.hpp"
#include <algorithm>
#include <queue>
#include <stdexcept>

namespace surreal {
std::string Event::canonical() const {
  std::string s="SURREAL_EVENT_V2\n";
  detail::append_field(s,"parent_count",std::to_string(parents.size()));
  for(const auto&p:parents)detail::append_field(s,"parent",p);
  detail::append_field(s,"depth",std::to_string(depth));
  detail::append_field(s,"time",local_time.str());
  detail::append_field(s,"region",region);
  detail::append_field(s,"law",law);
  detail::append_field(s,"before",before_hash);
  detail::append_field(s,"after",after_hash);
  detail::append_field(s,"invariant",invariant_hash);
  detail::append_field(s,"entropy",entropy_root);
  detail::append_field(s,"memo",memo_reuse?"1":"0");
  return s;
}
std::size_t CausalGraph::validate_admission(const std::vector<std::string>& parents,const Rational& local_time) const {
  if(parents.size()>kMaxEventParents)throw std::length_error("causal parent bound");
  std::size_t depth=0;Rational max_time(0);bool have=false;
  for(const auto&p:parents){auto it=events_.find(p);if(it==events_.end())throw std::invalid_argument("missing causal parent");depth=std::max(depth,it->second.depth+1);if(!have||it->second.local_time>max_time){max_time=it->second.local_time;have=true;}}
  if(have&&local_time<max_time)throw std::invalid_argument("causal time regression");
  return depth;
}
std::string CausalGraph::append(Event e){
  if(events_.size()>=kMaxCausalEvents)throw std::length_error("causal event bound");
  if(e.parents.size()>kMaxEventParents)throw std::length_error("causal parent bound");
  std::sort(e.parents.begin(),e.parents.end()); e.parents.erase(std::unique(e.parents.begin(),e.parents.end()),e.parents.end());
  e.depth=validate_admission(e.parents,e.local_time);
  e.id=hex(sha256(e.canonical()));
  if(events_.contains(e.id)) throw std::invalid_argument("duplicate causal event");
  events_.emplace(e.id,e);
  return e.id;
}
bool CausalGraph::contains(const std::string&id)const{return events_.contains(id);}
const Event& CausalGraph::at(const std::string&id)const{auto it=events_.find(id);if(it==events_.end())throw std::out_of_range("event");return it->second;}
std::vector<std::string> CausalGraph::descendants(const std::string&root)const{
  if(!contains(root))throw std::out_of_range("root event");
  std::map<std::string,std::vector<std::string>> children;
  for(const auto&[id,e]:events_)for(const auto&p:e.parents)children[p].push_back(id);
  std::set<std::string> seen;std::queue<std::string> q;q.push(root);
  while(!q.empty()){auto cur=q.front();q.pop();auto it=children.find(cur);if(it==children.end())continue;for(const auto&id:it->second)if(seen.insert(id).second)q.push(id);}
  return {seen.begin(),seen.end()};
}
std::string CausalGraph::root_hash()const{std::string s="SURREAL_CAUSAL_V2\n";for(const auto&[id,e]:events_){(void)e;detail::append_field(s,"event",id);}return hex(sha256(s));}
std::string causal_boundary_root(const CausalGraph&g,const std::vector<std::string>&parents){std::vector<std::string> p=parents;std::sort(p.begin(),p.end());p.erase(std::unique(p.begin(),p.end()),p.end());std::string s="SURREAL_CAUSAL_BOUNDARY_V2\n";for(const auto&id:p){if(!g.contains(id))throw std::invalid_argument("unknown boundary parent");detail::append_field(s,"parent",id);}return hex(sha256(s));}
}  // namespace surreal
