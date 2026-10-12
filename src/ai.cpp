#include "ai.hpp"
#include "neuro_sandbox.h"
#include "surreal/hash.hpp"
#include <array>
#include <charconv>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <poll.h>
#include <signal.h>
#include <stdexcept>
#include <string_view>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

namespace nm_ai {
namespace {
std::string digest(const std::string& s){return surreal::hex(surreal::sha256(s));}
std::string hex(const std::string& s){
    constexpr char symbols[]="0123456789abcdef";std::string out;out.reserve(s.size()*2);
    for(unsigned char c:s){out.push_back(symbols[c>>4]);out.push_back(symbols[c&15]);}return out;
}
std::string unhex(const std::string& s){
    if(s.size()%2)throw std::runtime_error("odd hex length");
    std::string out;out.reserve(s.size()/2);
    auto nib=[](char c)->int {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::runtime_error("noncanonical hex");};
    for(size_t i=0;i<s.size();i+=2)out.push_back(static_cast<char>((nib(s[i])<<4)|nib(s[i+1])));
    return out;
}
std::string file(const std::string& path){
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("cannot read local adapter");
    return {std::istreambuf_iterator<char>(in),{}};
}
std::string path_for(const std::string& path){
    auto p=std::filesystem::canonical(path);
    if(!std::filesystem::is_regular_file(p)||std::filesystem::file_size(p)>64*1024*1024||access(p.c_str(),X_OK))
        throw std::runtime_error("adapter must be an executable regular file of at most 64 MiB");
    for(unsigned char c:p.string())if(c<32||c==127)throw std::runtime_error("invalid adapter path");
    return p.string();
}
std::string request(const std::string& scenario){
    if(scenario.size()>1024*1024)throw std::runtime_error("scenario exceeds AI request capacity");
    return "NEUROMANTLE_AI_REQUEST_V1\nscenario_sha256="+digest(scenario)+"\nscenario_hex="+hex(scenario)+"\n";
}
std::string socket_for(const std::string& path){
    if(path.empty())return {};
    auto canonical=std::filesystem::canonical(path).string();
    if(canonical.size()>=sizeof(sockaddr_un::sun_path))throw std::runtime_error("Unix socket path too long");
    for(unsigned char c:canonical)if(c<32||c==127)throw std::runtime_error("invalid Unix socket path");
    struct stat st{};if(stat(canonical.c_str(),&st)||!S_ISSOCK(st.st_mode))throw std::runtime_error("endpoint is not a Unix socket");
    return canonical;
}
int connect_socket(const std::string& path){
    if(path.empty())return -1;
    int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
    if(fd<0)throw std::runtime_error("cannot create Unix socket");
    sockaddr_un address{};address.sun_family=AF_UNIX;
    std::memcpy(address.sun_path,path.c_str(),path.size()+1);
    if(connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address))){close(fd);throw std::runtime_error("cannot connect local model");}
    return fd;
}
std::string call(const std::string& executable,const std::string& input,const std::string& socket_path){
    char name[]="/tmp/neuromantle-ai-XXXXXX";
    int fd=mkstemp(name);if(fd<0)throw std::runtime_error("cannot stage AI request");
    (void)unlink(name);
    int model=-1;
    try{
        model=connect_socket(socket_path);
        size_t at=0;while(at<input.size()){
            ssize_t n=write(fd,input.data()+at,input.size()-at);
            if(n<0&&errno==EINTR)continue;
            if(n<=0)throw std::runtime_error("cannot write AI request");
            at+=static_cast<size_t>(n);
        }
        if(lseek(fd,0,SEEK_SET)<0)throw std::runtime_error("cannot rewind AI request");
        int p[2];if(pipe(p))throw std::runtime_error("cannot create AI output pipe");
        pid_t child=fork();
        if(child<0){close(p[0]);close(p[1]);throw std::runtime_error("cannot fork local adapter");}
        if(child==0){
            (void)setpgid(0,0);close(p[0]);
            if(dup2(fd,STDIN_FILENO)<0||dup2(p[1],STDOUT_FILENO)<0)_exit(127);
            if(model>=0){if(dup2(model,3)<0||fcntl(3,F_SETFD,0)<0)_exit(127);}
            if(fd!=3)close(fd);
            if(p[1]!=3)close(p[1]);
            if(model>=0&&model!=3)close(model);
            char* const args[]={const_cast<char*>(executable.c_str()),nullptr};
            if(nm_sandbox_child(model>=0?3:-1))_exit(126);
            execv(executable.c_str(),args);_exit(127);
        }
        close(fd);fd=-1;close(p[1]);if(model>=0){close(model);model=-1;}
        std::string output;bool failed=false;int status=0;
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
        for(;;){
            auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
            if(ms<=0){failed=true;break;}
            pollfd fds{p[0],POLLIN,0};int ready=poll(&fds,1,static_cast<int>(ms));
            if(ready<0&&errno==EINTR)continue;
            if(ready<=0){failed=true;break;}
            std::array<char,4096> chunk{};ssize_t n=read(p[0],chunk.data(),chunk.size());
            if(n<0&&errno==EINTR)continue;
            if(n<0){failed=true;break;}
            if(n==0)break;
            if(output.size()+static_cast<size_t>(n)>65536){failed=true;break;}
            output.append(chunk.data(),static_cast<size_t>(n));
        }
        close(p[0]);
        if(failed){(void)kill(-child,SIGKILL);(void)kill(child,SIGKILL);}
        for(;;){
            pid_t r=waitpid(child,&status,WNOHANG);if(r==child)break;
            if(r<0&&errno==EINTR)continue;
            if(r<0){failed=true;break;}
            if(std::chrono::steady_clock::now()>=deadline){failed=true;(void)kill(-child,SIGKILL);(void)kill(child,SIGKILL);
                while(waitpid(child,&status,0)<0&&errno==EINTR){}break;}
            (void)poll(nullptr,0,5);
        }
        if(failed||!WIFEXITED(status)||WEXITSTATUS(status)!=0)throw std::runtime_error("local adapter execution refused");
        return output;
    }catch(...){if(fd>=0)close(fd);if(model>=0)close(model);throw;}
}
template<class T>T number(std::string_view s){
    T x{};auto [p,e]=std::from_chars(s.data(),s.data()+s.size(),x);
    if(e!=std::errc{}||p!=s.data()+s.size()||std::to_string(x)!=s)throw std::runtime_error("noncanonical proposal integer");
    return x;
}
std::string proposal_directives(const std::string& proposal){
    constexpr std::string_view header="NEUROMANTLE_AI_PROPOSAL_V1\n";
    if(!proposal.starts_with(header)||proposal.size()>65536||proposal.back()!='\n')throw std::runtime_error("invalid AI proposal frame");
    std::string directives;size_t pos=header.size(),count=0;std::uint64_t prior_tick=0;std::uint16_t prior_id=0;
    while(pos<proposal.size()){
        size_t end=proposal.find('\n',pos);if(end==std::string::npos)throw std::runtime_error("proposal terminator missing");
        std::string_view line(proposal.data()+pos,end-pos);
        if(!line.starts_with("stim "))throw std::runtime_error("unsupported AI proposal operation");
        auto a=line.find(' ',5),b=a==std::string_view::npos?a:line.find(' ',a+1);
        if(a==std::string_view::npos||b==std::string_view::npos)throw std::runtime_error("invalid AI stimulus");
        auto tick=number<std::uint64_t>(line.substr(5,a-5));auto id=number<std::uint16_t>(line.substr(a+1,b-a-1));
        auto current=number<std::int64_t>(line.substr(b+1));
        if(current<=0||id>=1024||count>=4096||(count&&(tick<prior_tick||(tick==prior_tick&&id<=prior_id))))
            throw std::runtime_error("invalid or unordered AI stimulus");
        prior_tick=tick;prior_id=id;count++;directives.append(line);directives.push_back('\n');pos=end+1;
    }
    if(!count)throw std::runtime_error("empty AI proposal");
    return directives;
}
std::string inner(const std::string& scenario,const std::string& proposal,const Decision& decide){
    try{return decide(scenario+"\n"+proposal_directives(proposal));}
    catch(const std::exception& e){std::string part="NEUROMANTLE_AI_REFUSAL_V1\nreason="+std::string(e.what())+"\n";
        return part+"receipt_root="+digest(part)+"\n";}
}
std::string frame(const std::string& scenario,const std::string& adapter,const std::string& adapter_hash,
                  const std::string& socket_path,const std::string& proposal,const Decision& decide){
    auto result=inner(scenario,proposal,decide);
    std::string status=result.starts_with("NEUROMANTLE_RECEIPT_V1\n")?"ADMIT":"REFUSE";
    std::string prefix="NEUROMANTLE_AI_RECEIPT_V1\nstatus="+status+"\nscenario="+digest(scenario)+"\nadapter="+adapter+
        "\nadapter_sha256="+adapter_hash+"\nsocket_path="+socket_path+"\nproposal_sha256="+digest(proposal)+
        "\nproposal_hex="+hex(proposal)+"\ninner_hex="+hex(result)+"\n";
    return prefix+"receipt_root="+digest(prefix)+"\n";
}
std::string field(const std::string& input,size_t& pos,std::string_view key){
    std::string prefix(key);prefix+='=';
    if(input.compare(pos,prefix.size(),prefix)!=0)throw std::runtime_error("AI receipt field mismatch");
    pos+=prefix.size();
    auto end=input.find('\n',pos);if(end==std::string::npos)throw std::runtime_error("unterminated AI receipt");
    auto value=input.substr(pos,end-pos);pos=end+1;return value;
}
}
std::string run(const std::string& scenario,const std::string& executable,const Decision& decide,const std::string& socket_path){
    auto path=path_for(executable);auto hash=digest(file(path));
    auto endpoint=socket_for(socket_path);
    auto proposal=call(path,request(scenario),endpoint);
    if(digest(file(path))!=hash)throw std::runtime_error("adapter changed during execution");
    return frame(scenario,path,hash,endpoint,proposal,decide);
}
void verify(const std::string& scenario,const std::string& receipt,const Decision& decide,bool live){
    constexpr std::string_view header="NEUROMANTLE_AI_RECEIPT_V1\n";
    if(!receipt.starts_with(header))throw std::runtime_error("invalid AI receipt");
    size_t pos=header.size();auto status=field(receipt,pos,"status");auto scenario_hash=field(receipt,pos,"scenario");
    auto adapter=field(receipt,pos,"adapter"),hash=field(receipt,pos,"adapter_sha256");
    auto socket_path=field(receipt,pos,"socket_path");
    auto proposal_hash=field(receipt,pos,"proposal_sha256"),proposal=unhex(field(receipt,pos,"proposal_hex"));
    auto result=unhex(field(receipt,pos,"inner_hex"));auto root=field(receipt,pos,"receipt_root");
    if(pos!=receipt.size()||(status!="ADMIT"&&status!="REFUSE")||scenario_hash!=digest(scenario)||proposal_hash!=digest(proposal)||
       root!=digest(receipt.substr(0,receipt.rfind("receipt_root="))))
        throw std::runtime_error("AI receipt integrity mismatch");
    if(path_for(adapter)!=adapter||digest(file(adapter))!=hash)throw std::runtime_error("AI adapter changed");
    if(inner(scenario,proposal,decide)!=result||frame(scenario,adapter,hash,socket_path,proposal,decide)!=receipt)
        throw std::runtime_error("AI admission replay mismatch");
    if(live && run(scenario,adapter,decide,socket_path)!=receipt)throw std::runtime_error("live adapter output differs");
}
}
