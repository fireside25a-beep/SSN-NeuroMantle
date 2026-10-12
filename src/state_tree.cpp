#include "state_tree.hpp"
#include "neuro.h"
#include "mm/spatial_pool.h"
#include "surreal/hash.hpp"
#include <cstdint>
#include <stdexcept>

namespace nm_state {
namespace {
std::string digest(const std::string &data){return surreal::hex(surreal::sha256(data));}
constexpr std::size_t neuron_leaves=16, edge_leaves=64, pending_leaves=NM_MAX_DELAY+1;
constexpr std::size_t pending_first=neuron_leaves+edge_leaves;
constexpr std::size_t spatial_first=pending_first+pending_leaves;
constexpr std::size_t tick_leaf=spatial_first+16;
}
Tree::Tree(const nm_network &n):network(n){
    for(std::size_t i=0;i<leaf_count;++i)nodes[leaf_count+i]=leaf(i);
    for(std::size_t i=leaf_count-1;i>0;--i)
        nodes[i]=digest("NM_NODE_V1\n"+nodes[i*2]+nodes[i*2+1]);
}
void Tree::mark(std::size_t index){dirty[leaf_count+index]=true;}
void Tree::neuron(std::size_t id){mark(id/64);}
void Tree::edge(std::size_t id){mark(neuron_leaves+id/256);}
void Tree::pending(std::size_t slot){mark(pending_first+slot);}
void Tree::tick(){mark(tick_leaf);}
std::string Tree::leaf(std::size_t index) const {
    std::string frame="NM_LEAF_V1\n"+std::to_string(index)+"\n";
    if(index<neuron_leaves){
        for(std::size_t i=index*64;i<(index+1)*64;++i){const auto &v=network.neurons[i];if(!v.configured)continue;
            frame+=std::to_string(i)+":"+std::to_string(v.voltage)+":"+std::to_string(v.remaining)+":"+
                   std::to_string(v.spikes)+":"+std::to_string(v.last_spike)+"\n";}
    }else if(index<pending_first){
        for(std::size_t i=(index-neuron_leaves)*256;i<(index-neuron_leaves+1)*256&&i<network.edge_count;++i){
            const auto &e=network.edges[i];std::uint32_t weight=0,count=0;
            if(neuro_get_edge_weight(e.src,std::uint64_t(e.dst)+1,&weight)||
               neuro_get_edge_spike_count(e.src,std::uint64_t(e.dst)+1,&count))
                throw std::runtime_error("router state unavailable");
            frame+=std::to_string(i)+":"+std::to_string(e.src)+":"+std::to_string(e.dst)+":"+
                   std::to_string(e.weight)+":"+std::to_string(weight)+":"+std::to_string(count)+"\n";
        }
    }else if(index<spatial_first){
        const auto slot=index-pending_first;
        for(std::size_t i=0;i<NM_MAX_NEURONS;++i)if(network.pending[slot][i])
            frame+=std::to_string(i)+":"+std::to_string(network.pending[slot][i])+"\n";
    }else if(index<tick_leaf){
        for(std::size_t i=(index-spatial_first)*64;i<(index-spatial_first+1)*64;++i){
            struct sparse_memory_chunk chunk{};
            if(spatial_pool_get_chunk(i,&chunk)==0)
                frame+=std::to_string(i)+":"+std::to_string(chunk.physical_page_pointer)+":"+
                       std::to_string(chunk.spatial_density_score)+"\n";
        }
    }else if(index==tick_leaf){frame+=std::to_string(network.tick)+"\n";}
    return digest(frame);
}
std::string Tree::root(){
    for(std::size_t i=leaf_count;i<2*leaf_count;++i)if(dirty[i]){
        nodes[i]=leaf(i-leaf_count);dirty[i]=false;dirty[i/2]=true;
    }
    for(std::size_t i=leaf_count-1;i>0;--i)if(dirty[i]){
        nodes[i]=digest("NM_NODE_V1\n"+nodes[i*2]+nodes[i*2+1]);dirty[i]=false;
        if(i>1)dirty[i/2]=true;
    }
    return nodes[1];
}
}
