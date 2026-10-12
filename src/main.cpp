#include "neuro_mantle.h"
#include "neuro_mantle_ecc.h"
#include "neuro_sandbox.h"
#include "ai.hpp"
#include "state_tree.hpp"
#include "neuro.h"
#include "mm/spatial_pool.h"
#include "surreal/hash.hpp"
#include "surreal/causal.hpp"
#include "surreal/entropy.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <memory>
#include <map>
#include <poll.h>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <vector>
#include <sys/stat.h>
#include <sys/random.h>
#include <sys/wait.h>
#include <chrono>
#include <unistd.h>

extern "C" int surreal_getrandom_u64(std::uint64_t *out) {
    if(!out)return -1;
    auto *bytes=reinterpret_cast<unsigned char*>(out);size_t done=0;
    while(done<sizeof(*out)){
        ssize_t count=getrandom(bytes+done,sizeof(*out)-done,0);
        if(count<0 && errno==EINTR)continue;
        if(count<=0)return -1;
        done+=static_cast<size_t>(count);
    }
    return 0;
}
namespace {
struct Stim {std::uint64_t tick; std::uint16_t neuron; std::int64_t current;};
struct Noise {std::uint64_t tick; std::uint16_t neuron; std::int64_t amplitude;};
struct Module {std::string path,sha256;};
struct Scenario {
    std::unique_ptr<nm_network> net=std::make_unique<nm_network>();
    std::vector<Stim> stimuli;
    std::vector<Noise> noise;
    std::map<std::uint16_t,Module> modules;
    std::uint64_t seed=0, steps=0;
    bool has_seed=false, has_steps=false;
    bool ecc=false;
    std::string qecc_path,qecc_hash;
};
std::filesystem::path runtime_root;
std::string read_file(const std::string &path) {
    std::ifstream f(path,std::ios::binary); if(!f)throw std::runtime_error("cannot open "+path);
    return {std::istreambuf_iterator<char>(f),{}};
}
template<class T>T integer(const std::string &s);
std::int64_t invoke(const Module &module,std::uint64_t tick,std::uint16_t src,std::uint16_t dst){
    int pipefd[2];if(pipe(pipefd))throw std::system_error(errno,std::generic_category(),"module pipe");
    pid_t pid=fork();
    if(pid<0){close(pipefd[0]);close(pipefd[1]);throw std::system_error(errno,std::generic_category(),"module fork");}
    if(pid==0){
        (void)setpgid(0,0);close(pipefd[0]);
        int input=open("/dev/null",O_RDONLY);
        if(input>=0){(void)dup2(input,STDIN_FILENO);close(input);}
        if(dup2(pipefd[1],STDOUT_FILENO)<0)_exit(127);
        close(pipefd[1]);
        std::string t=std::to_string(tick),s=std::to_string(src),d=std::to_string(dst);
        char *const args[]={const_cast<char*>(module.path.c_str()),t.data(),s.data(),d.data(),nullptr};
        if(nm_sandbox_child(-1))_exit(126);
        execv(module.path.c_str(),args);_exit(127);
    }
    close(pipefd[1]);
    std::string output;int status=0;bool failed=false;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    for(;;){
        auto remain=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        if(remain<=0){failed=true;break;}
        pollfd fds{pipefd[0],POLLIN,0};int ready=poll(&fds,1,static_cast<int>(remain));
        if(ready<0&&errno==EINTR)continue;
        if(ready<=0){failed=true;break;}
        char buffer[64];ssize_t n=read(pipefd[0],buffer,sizeof(buffer));
        if(n<0&&errno==EINTR)continue;
        if(n<0){failed=true;break;}
        if(n==0)break;
        if(output.size()+static_cast<size_t>(n)>64){failed=true;break;}
        output.append(buffer,static_cast<size_t>(n));
    }
    close(pipefd[0]);
    if(failed){(void)kill(-pid,SIGKILL);(void)kill(pid,SIGKILL);}
    for(;;){
        auto r=waitpid(pid,&status,WNOHANG);
        if(r==pid)break;
        if(r<0&&errno==EINTR)continue;
        if(r<0){failed=true;break;}
        if(std::chrono::steady_clock::now()>=deadline){
            failed=true;(void)kill(-pid,SIGKILL);(void)kill(pid,SIGKILL);
            while(waitpid(pid,&status,0)<0&&errno==EINTR){}
            break;
        }
        (void)poll(nullptr,0,5);
    }
    if(failed||!WIFEXITED(status)||WEXITSTATUS(status)!=0||output.empty()||output.back()!='\n')
        throw std::runtime_error("module execution refused");
    output.pop_back();
    try {return integer<std::int64_t>(output);}
    catch(const std::exception &){throw std::runtime_error("invalid module output");}
}
std::string verify_qecc(const std::string &path){
    auto tool=(runtime_root/"vendor/surreal/upstream/bin/surreal-linux-x86_64").string();
    int pipefd[2];if(pipe(pipefd))throw std::runtime_error("cannot create QECC verification pipe");
    pid_t pid=fork();
    if(pid<0){close(pipefd[0]);close(pipefd[1]);throw std::runtime_error("cannot fork QECC verifier");}
    if(pid==0){
        (void)setpgid(0,0);close(pipefd[0]);
        if(dup2(pipefd[1],STDOUT_FILENO)<0)_exit(127);
        close(pipefd[1]);char *const args[]={tool.data(),const_cast<char*>("qec-verify"),const_cast<char*>(path.c_str()),nullptr};
        if(nm_sandbox_child(-1))_exit(126);
        execv(tool.c_str(),args);_exit(127);
    }
    close(pipefd[1]);std::string output;bool failed=false;int status=0;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for(;;){
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        if(ms<=0){failed=true;break;}
        pollfd fds{pipefd[0],POLLIN,0};int ready=poll(&fds,1,static_cast<int>(ms));
        if(ready<0&&errno==EINTR)continue;
        if(ready<=0){failed=true;break;}
        char buf[1024];ssize_t n=read(pipefd[0],buf,sizeof(buf));
        if(n<0&&errno==EINTR)continue;
        if(n<0){failed=true;break;}
        if(n==0)break;
        if(output.size()+static_cast<size_t>(n)>16384){failed=true;break;}
        output.append(buf,static_cast<size_t>(n));
    }
    close(pipefd[0]);if(failed){(void)kill(-pid,SIGKILL);(void)kill(pid,SIGKILL);}
    for(;;){auto r=waitpid(pid,&status,WNOHANG);if(r==pid)break;
        if(r<0&&errno==EINTR)continue;
        if(r<0){failed=true;break;}
        if(std::chrono::steady_clock::now()>=deadline){failed=true;(void)kill(-pid,SIGKILL);(void)kill(pid,SIGKILL);
            while(waitpid(pid,&status,0)<0&&errno==EINTR){}break;}
        (void)poll(nullptr,0,5);
    }
    if(failed||!WIFEXITED(status)||WEXITSTATUS(status)!=0||
       !output.starts_with("SURREAL_QECC_PROOF_V1\nvalid:1:1\nqubits:1:5\nstabilizer_rank:1:4\nlogical_qubits:1:1\ndistance_lower_bound:1:3\ndistance_upper_bound:1:3\n"))
        throw std::runtime_error("five-qubit QECC certificate refused");
    return surreal::hex(surreal::sha256(output));
}
void write_file(const std::string &path,const std::string &data) {
    std::string tmp=path+".tmp.XXXXXX";
    int fd=mkstemp(tmp.data());
    if(fd<0)throw std::system_error(errno,std::generic_category(),"create receipt");
    bool committed=false;
    try {
        if(fchmod(fd,0644))throw std::system_error(errno,std::generic_category(),"receipt permissions");
        size_t at=0;
        while(at<data.size()){
            ssize_t count=write(fd,data.data()+at,data.size()-at);
            if(count<0 && errno==EINTR)continue;
            if(count<=0)throw std::system_error(errno,std::generic_category(),"write receipt");
            at+=static_cast<size_t>(count);
        }
        if(fsync(fd))throw std::system_error(errno,std::generic_category(),"sync receipt");
        if(close(fd))throw std::system_error(errno,std::generic_category(),"close receipt");
        fd=-1;
        if(rename(tmp.c_str(),path.c_str()))throw std::system_error(errno,std::generic_category(),"commit receipt");
        committed=true;
        auto parent=std::filesystem::path(path).parent_path();
        int dir=open((parent.empty()?std::filesystem::path("."):parent).c_str(),O_RDONLY|O_DIRECTORY);
        if(dir>=0){(void)fsync(dir);(void)close(dir);}
    }catch(...){if(fd>=0)(void)close(fd);if(!committed)(void)unlink(tmp.c_str());throw;}
}
template<class T>T integer(const std::string &s) {
    T v{}; auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),v);
    if(e!=std::errc{}||p!=s.data()+s.size())throw std::runtime_error("invalid integer: "+s);
    return v;
}
template<class T>T word(std::istringstream &in) {
    std::string s; if(!(in>>s))throw std::runtime_error("missing argument");return integer<T>(s);
}
void end(std::istringstream &in) {std::string s;if(in>>s)throw std::runtime_error("extra argument: "+s);}
std::array<std::uint64_t,16> key(const std::string &hex) {
    if(hex.size()!=256)throw std::runtime_error("spatial key requires 256 hex digits");
    std::array<std::uint64_t,16> words{};
    for(size_t i=0;i<16;++i){auto part=hex.substr(i*16,16);auto [p,e]=std::from_chars(part.data(),part.data()+16,words[i],16);
        if(e!=std::errc{}||p!=part.data()+16)throw std::runtime_error("invalid spatial key");}
    return words;
}
Scenario parse(const std::string &data) {
    Scenario s;nm_init(s.net.get());neuro_router_reset();spatial_pool_reset();
    std::istringstream source(data);std::string line;std::size_t line_no=0;
    while(std::getline(source,line)){
        ++line_no;if(line.empty()||line[0]=='#')continue;
        try {
            std::istringstream in(line);std::string op;in>>op;
            if(op=="neuron"){
                auto id=word<std::uint16_t>(in);auto reset=word<std::int64_t>(in);auto threshold=word<std::int64_t>(in);
                auto leak=word<std::uint32_t>(in);auto refr=word<std::uint32_t>(in);end(in);
                if(nm_add_neuron(s.net.get(),id,reset,threshold,leak,refr))throw std::runtime_error("invalid neuron");
                if(neuro_set_node(id,std::uint64_t(id)+1))throw std::runtime_error("router node failure");
            }else if(op=="edge"){
                auto a=word<std::uint16_t>(in),b=word<std::uint16_t>(in);
                auto w=word<std::int32_t>(in);auto delay=word<std::uint16_t>(in);
                auto plus=word<std::uint16_t>(in),minus=word<std::uint16_t>(in),window=word<std::uint16_t>(in);
                auto lo=word<std::int32_t>(in),hi=word<std::int32_t>(in);end(in);
                if(nm_add_edge(s.net.get(),a,b,w,delay,plus,minus,window,lo,hi))throw std::runtime_error("invalid synapse");
                if(neuro_add_edge(a,std::uint64_t(b)+1,static_cast<std::uint32_t>(w>0?w:0)))throw std::runtime_error("router edge failure");
            }else if(op=="feature"){
                auto index=word<std::uint16_t>(in),target=word<std::uint16_t>(in);std::string bits;if(!(in>>bits))throw std::runtime_error("missing feature key");end(in);
                if(target>=NM_MAX_NEURONS||!s.net->neurons[target].configured)throw std::runtime_error("unknown feature target");
                auto v=key(bits);if(spatial_pool_set_chunk(index,v.data(),std::uint64_t(target)+1,0))throw std::runtime_error("invalid feature");
            }else if(op=="stim"||op=="noise"){
                auto tick=word<std::uint64_t>(in);auto id=word<std::uint16_t>(in);auto value=word<std::int64_t>(in);end(in);
                if(id>=NM_MAX_NEURONS||!s.net->neurons[id].configured||value<0)throw std::runtime_error("invalid stimulus");
                if(op=="stim")s.stimuli.push_back({tick,id,value});else s.noise.push_back({tick,id,value});
            }else if(op=="stim_feature"){
                auto tick=word<std::uint64_t>(in);std::string bits;if(!(in>>bits))throw std::runtime_error("missing feature key");
                auto current=word<std::int64_t>(in);end(in);auto v=key(bits);
                auto page=allocate_spatial_memory(v.data(),v.size());
                if(!page||page>NM_MAX_NEURONS||current<0)throw std::runtime_error("no valid spatial match");
                s.stimuli.push_back({tick,static_cast<std::uint16_t>(page-1),current});
            }else if(op=="module"){
                auto id=word<std::uint16_t>(in);std::string path;if(!(in>>path))throw std::runtime_error("missing module path");end(in);
                if(id>=NM_MAX_NEURONS||!s.net->neurons[id].configured||s.modules.contains(id))throw std::runtime_error("invalid module target");
                auto actual=std::filesystem::canonical(path);
                for(unsigned char c:actual.string())if(c<32||c==127)throw std::runtime_error("invalid module path");
                if(access(actual.c_str(),X_OK))throw std::runtime_error("module is not executable");
                if(std::filesystem::file_size(actual)>64*1024*1024)throw std::runtime_error("module file too large");
                s.modules.emplace(id,Module{actual.string(),surreal::hex(surreal::sha256(read_file(actual.string())))});
            }else if(op=="ecc"){
                std::string type;if(!(in>>type))throw std::runtime_error("missing ECC mode");end(in);
                if(s.ecc||type!="secded72")throw std::runtime_error("invalid ECC mode");
                s.ecc=true;
            }else if(op=="qecc_certificate"){
                std::string path;if(!(in>>path))throw std::runtime_error("missing QECC certificate");end(in);
                if(!s.qecc_path.empty())throw std::runtime_error("duplicate QECC certificate");
                auto actual=std::filesystem::canonical(path);
                if(!std::filesystem::is_regular_file(actual)||std::filesystem::file_size(actual)>1024*1024)
                    throw std::runtime_error("invalid QECC certificate file");
                for(unsigned char c:actual.string())if(c<32||c==127)throw std::runtime_error("invalid QECC path");
                s.qecc_path=actual.string();s.qecc_hash=surreal::hex(surreal::sha256(read_file(s.qecc_path)));
            }else if(op=="seed") {s.seed=word<std::uint64_t>(in);end(in);if(s.has_seed)throw std::runtime_error("duplicate seed");s.has_seed=true;
            }else if(op=="steps") {s.steps=word<std::uint64_t>(in);end(in);if(s.has_steps||s.steps>1000000)throw std::runtime_error("invalid steps");s.has_steps=true;
            }else throw std::runtime_error("unknown operation: "+op);
        }catch(const std::exception &e){throw std::runtime_error("line "+std::to_string(line_no)+": "+e.what());}
    }
    if(!s.has_steps||(!s.noise.empty()&&!s.has_seed))throw std::runtime_error("steps or noise seed missing");
    for(const auto &stim:s.stimuli)if(stim.tick>=s.steps)throw std::runtime_error("stimulus beyond simulation");
    for(const auto &noise:s.noise)if(noise.tick>=s.steps)throw std::runtime_error("noise beyond simulation");
    return s;
}
struct PlasticityRecord {
    nm_edge edge;
    std::uint64_t tick,last_src,last_dst;
    std::int32_t actual;
    bool fired_src,fired_dst;
};
void audit_plasticity(const std::vector<PlasticityRecord> &records){
    std::map<std::tuple<std::uint16_t,std::uint16_t,std::uint64_t>,std::int64_t> quotient_cache;
    auto exact=[&](std::uint16_t rate,std::uint16_t window,std::uint64_t distance){
        auto key=std::make_tuple(rate,window,distance);
        auto found=quotient_cache.find(key);if(found!=quotient_cache.end())return found->second;
        surreal::Rational change(static_cast<long long>(rate)*static_cast<long long>(window-distance+1));
        change=change/surreal::Rational(window);
        std::int64_t value=std::stoll((change.num()/change.den()).convert_to<std::string>());
        quotient_cache.emplace(key,value);return value;
    };
    for(const auto &record:records){
        const auto &e=record.edge;
        std::int64_t expected=e.weight;
        if(record.fired_src && record.last_dst && record.tick+1-record.last_dst<=e.window){
            auto distance=record.tick+1-record.last_dst;
            expected-=exact(e.minus,e.window,distance);
        }
        if(record.fired_dst && record.last_src && record.tick+1-record.last_src<=e.window){
            auto distance=record.tick+1-record.last_src;
            expected+=exact(e.plus,e.window,distance);
        }
        expected=std::max<std::int64_t>(e.min_weight,std::min<std::int64_t>(e.max_weight,expected));
        if(record.actual!=expected)throw std::runtime_error("exact plasticity audit failed");
    }
}
std::string execute(const std::string &data) {
    auto s=parse(data);surreal::EntropyLedger entropy(s.seed);surreal::CausalGraph graph;
    std::string receipt="NEUROMANTLE_RECEIPT_V1\nscenario="+surreal::hex(surreal::sha256(data))+"\n";
    if(!s.qecc_path.empty()){
        auto verifier=(runtime_root/"vendor/surreal/upstream/bin/surreal-linux-x86_64").string();
        auto verifier_hash=surreal::hex(surreal::sha256(read_file(verifier)));
        auto proof=verify_qecc(s.qecc_path);
        if(s.qecc_hash!=surreal::hex(surreal::sha256(read_file(s.qecc_path))))throw std::runtime_error("QECC certificate changed");
        if(verifier_hash!=surreal::hex(surreal::sha256(read_file(verifier))))throw std::runtime_error("QECC verifier changed");
        receipt+="qecc_certificate="+s.qecc_path+":"+s.qecc_hash+":"+proof+":"+verifier_hash+"\n";
    }
    std::vector<std::uint8_t> checkpoint;
    if(s.ecc){checkpoint.resize(nm_ecc_checkpoint_size());
        if(nm_ecc_capture(s.net.get(),checkpoint.data(),checkpoint.size()))throw std::runtime_error("ECC initial capture failed");
        receipt+="ecc=secded72\n";
    }
    for(const auto &[id,module]:s.modules)
        receipt+="module="+std::to_string(id)+":"+module.path+":"+module.sha256+"\n";
    std::string prior;
    std::vector<PlasticityRecord> audit_batch;
    nm_state::Tree state(*s.net);
    std::map<std::uint32_t,size_t> edge_index;
    for(size_t i=0;i<s.net->edge_count;++i)
        edge_index.emplace(std::uint32_t(s.net->edges[i].src)*NM_MAX_NEURONS+s.net->edges[i].dst,i);
    for(std::uint64_t tick=0;tick<s.steps;++tick){
        size_t corrected=0;
        if(s.ecc&&nm_ecc_restore(checkpoint.data(),checkpoint.size(),s.net.get(),&corrected))
            throw std::runtime_error("ECC checkpoint refused");
        for(const auto &stim:s.stimuli)if(stim.tick==tick){
            if(nm_inject(s.net.get(),stim.neuron,tick,stim.current))throw std::runtime_error("stimulus overflow");
            state.pending(tick%(NM_MAX_DELAY+1));
        }
        for(const auto &noise:s.noise)if(noise.tick==tick){
            auto draw=entropy.draw();auto value=static_cast<std::int64_t>(draw%(static_cast<std::uint64_t>(noise.amplitude)+1));
            if(nm_inject(s.net.get(),noise.neuron,tick,value))throw std::runtime_error("noise overflow");
            state.pending(tick%(NM_MAX_DELAY+1));
        }
        auto before=state.root();nm_spikes spikes{};
        std::array<std::uint64_t,NM_MAX_NEURONS> previous{};
        for(size_t i=0;i<NM_MAX_NEURONS;++i)previous[i]=s.net->neurons[i].last_spike;
        std::vector<std::int32_t> old_weights;
        old_weights.reserve(s.net->edge_count);
        for(size_t i=0;i<s.net->edge_count;++i)old_weights.push_back(s.net->edges[i].weight);
        if(nm_step(s.net.get(),&spikes))throw std::runtime_error("neuron transition overflow");
        state.tick();state.pending(tick%(NM_MAX_DELAY+1));
        for(size_t i=0;i<NM_MAX_NEURONS;++i)if(s.net->neurons[i].configured)state.neuron(i);
        std::array<unsigned char,NM_MAX_NEURONS> fired{};
        for(size_t i=0;i<spikes.count;++i)fired[spikes.ids[i]]=1;
        for(size_t i=0;i<s.net->edge_count;++i){
            const auto &e=s.net->edges[i];
            bool recent_pre=fired[e.src]&&previous[e.dst]&&tick+1-previous[e.dst]<=e.window;
            bool recent_post=fired[e.dst]&&previous[e.src]&&tick+1-previous[e.src]<=e.window;
            if(recent_pre||recent_post||e.weight!=old_weights[i]){
                auto before=e;before.weight=old_weights[i];
                audit_batch.push_back({before,tick,previous[e.src],previous[e.dst],e.weight,
                                       fired[e.src]!=0,fired[e.dst]!=0});
            }
            if(e.weight!=old_weights[i])state.edge(i);
            if(fired[e.src])state.pending((tick+e.delay)%(NM_MAX_DELAY+1));
        }
        if((tick+1)%10==0||tick+1==s.steps){audit_plasticity(audit_batch);audit_batch.clear();}
        for(size_t i=0;i<s.net->edge_count;++i){
            const auto &e=s.net->edges[i];
            if(neuro_update_edge_weight(e.src,std::uint64_t(e.dst)+1,
                    static_cast<std::uint32_t>(e.weight>0?e.weight:0)))throw std::runtime_error("router plasticity update failed");
        }
        std::string spike_ids;
        std::string module_outputs;
        for(size_t i=0;i<spikes.count;++i){
            auto id=spikes.ids[i];auto dest=route_synaptic_signal(id,NEURO_ACTIVATION_THRESHOLD);
            if(dest&&dest<=NM_MAX_NEURONS){
                auto found=edge_index.find(std::uint32_t(id)*NM_MAX_NEURONS+static_cast<std::uint32_t>(dest-1));
                if(found!=edge_index.end())state.edge(found->second);
            }
            if(i)spike_ids+=",";
            spike_ids+=std::to_string(id)+">"+std::to_string(dest);
            if(dest && tick+1<s.steps && s.modules.contains(static_cast<std::uint16_t>(dest-1))){
                auto target=static_cast<std::uint16_t>(dest-1);
                auto current=invoke(s.modules.at(target),tick,id,target);
                if(nm_inject(s.net.get(),target,tick+1,current))throw std::runtime_error("module current overflow");
                state.pending((tick+1)%(NM_MAX_DELAY+1));
                if(!module_outputs.empty())module_outputs+=",";
                module_outputs+=std::to_string(id)+">"+std::to_string(target)+":"+std::to_string(current);
            }
        }
        auto after=state.root();
        std::string ecc_hash;
        if(s.ecc){
            if(nm_ecc_capture(s.net.get(),checkpoint.data(),checkpoint.size()))throw std::runtime_error("ECC checkpoint capture failed");
            ecc_hash=surreal::hex(surreal::sha256(std::string(reinterpret_cast<const char*>(checkpoint.data()),checkpoint.size())));
        }
        surreal::Event event;event.local_time=surreal::Rational(static_cast<long long>(tick));
        event.region="cpu";event.law="neuro.lif_stdp";event.before_hash=before;event.after_hash=after;
        event.invariant_hash=surreal::hex(surreal::sha256(spike_ids+"|"+module_outputs));event.entropy_root=entropy.root();
        if(!prior.empty())event.parents.push_back(prior);
        prior=graph.append(std::move(event));
        receipt+="tick="+std::to_string(tick)+";spikes="+spike_ids+";modules="+module_outputs+
                 ";state="+after+";event="+prior;
        if(s.ecc)receipt+=";ecc="+ecc_hash+":"+std::to_string(corrected);
        receipt+="\n";
    }
    receipt+="causal_root="+graph.root_hash()+"\nentropy_root="+entropy.root()+"\n";
    receipt+="receipt_root="+surreal::hex(surreal::sha256(receipt))+"\n";
    return receipt;
}
std::string decide(const std::string &data){
    try {return execute(data);}
    catch(const std::exception &e){
        std::string frame="NEUROMANTLE_REFUSAL_V1\nscenario="+
            surreal::hex(surreal::sha256(data))+"\nreason="+e.what()+"\n";
        return frame+"receipt_root="+surreal::hex(surreal::sha256(frame))+"\n";
    }
}
void usage(){std::cerr<<"usage: neuromantle run SCENARIO RECEIPT | verify SCENARIO RECEIPT | ai-run SCENARIO LOCAL_EXECUTABLE RECEIPT | ai-run-socket SCENARIO LOCAL_EXECUTABLE UNIX_SOCKET RECEIPT | ai-verify SCENARIO RECEIPT | ai-verify-live SCENARIO RECEIPT | dev COMMAND ... | surreal COMMAND ... | compile-vanta PROGRAM OUTPUT\n";}
}
int main(int argc,char **argv){
    try {
        runtime_root=std::filesystem::canonical(argv[0]).parent_path().parent_path();
        if(argc>=3 && std::string(argv[1])=="dev"){
            auto root=std::filesystem::canonical(argv[0]).parent_path().parent_path();
            auto tool=(root/"vendor/devcli/bin/neuro").string();
            std::vector<char*> args;args.push_back(tool.data());
            for(int i=2;i<argc;++i)args.push_back(argv[i]);
            args.push_back(nullptr);
            execv(tool.c_str(),args.data());throw std::runtime_error("cannot execute developer CLI");
        }
        if(argc>=3 && std::string(argv[1])=="surreal"){
            auto root=std::filesystem::canonical(argv[0]).parent_path().parent_path();
            auto tool=(root/"vendor/surreal/upstream/bin/surreal-linux-x86_64").string();
            std::vector<char*> args;args.push_back(tool.data());
            for(int i=2;i<argc;++i)args.push_back(argv[i]);
            args.push_back(nullptr);
            execv(tool.c_str(),args.data());throw std::runtime_error("cannot execute Surreal tool");
        }
        if(argc==4 && std::string(argv[1])=="compile-vanta"){
            auto root=std::filesystem::canonical(argv[0]).parent_path().parent_path();
            auto tool=(root/"vendor/vanta/crosspiler/vantax.py").string();
            std::string python="python3",output=argv[3];
            char *const args[]={python.data(),tool.data(),argv[2],const_cast<char*>("-o"),output.data(),nullptr};
            execvp(python.c_str(),args);throw std::runtime_error("cannot execute Vanta compiler");
        }
        if(argc==5 && std::string(argv[1])=="ai-run"){
            auto result=nm_ai::run(read_file(argv[2]),argv[3],decide);
            write_file(argv[4],result);std::cout<<result;
            return result.starts_with("NEUROMANTLE_AI_RECEIPT_V1\nstatus=ADMIT\n")?0:3;
        }
        if(argc==6 && std::string(argv[1])=="ai-run-socket"){
            auto result=nm_ai::run(read_file(argv[2]),argv[3],decide,argv[4]);
            write_file(argv[5],result);std::cout<<result;
            return result.starts_with("NEUROMANTLE_AI_RECEIPT_V1\nstatus=ADMIT\n")?0:3;
        }
        if(argc==4 && (std::string(argv[1])=="ai-verify"||std::string(argv[1])=="ai-verify-live")){
            nm_ai::verify(read_file(argv[2]),read_file(argv[3]),decide,std::string(argv[1])=="ai-verify-live");
            std::cout<<"AI admission replay: PASS\n";return 0;
        }
        if(argc!=4){usage();return 2;}
        auto result=decide(read_file(argv[2]));std::string op=argv[1];
        if(op=="run"){write_file(argv[3],result);std::cout<<result;
            return result.starts_with("NEUROMANTLE_REFUSAL_V1")?3:0;}
        if(op=="verify"){
            if(read_file(argv[3])!=result)throw std::runtime_error("receipt differs from deterministic replay");
            std::cout<<"receipt replay: PASS\n";return 0;
        }
        usage();return 2;
    }catch(const std::exception &e){std::cerr<<"neuromantle: "<<e.what()<<"\n";return 1;}
}
