#include "state_tree.hpp"
#include "neuro.h"
#include "mm/spatial_pool.h"
#include <cassert>
#include <iostream>
#include <memory>

int main(){
    neuro_router_reset();spatial_pool_reset();
    auto network=std::make_unique<nm_network>();nm_init(network.get());
    assert(nm_add_neuron(network.get(),0,0,10,65536,0)==0);
    assert(nm_add_neuron(network.get(),1,0,10,65536,0)==0);
    assert(nm_add_edge(network.get(),0,1,5,1,4,3,4,0,20)==0);
    assert(neuro_set_node(0,1)==0);
    assert(neuro_add_edge(0,2,5)==0);
    nm_state::Tree tree(*network);
    auto check=[&]{assert(tree.root()==nm_state::Tree(*network).root());};
    check();
    for(unsigned tick=0;tick<25;++tick){
        assert(nm_inject(network.get(),0,tick,10)==0);
        tree.pending(tick%(NM_MAX_DELAY+1));check();
        nm_spikes spikes{};
        assert(nm_step(network.get(),&spikes)==0);
        tree.tick();tree.neuron(0);tree.neuron(1);tree.pending(tick%(NM_MAX_DELAY+1));
        tree.pending((tick+1)%(NM_MAX_DELAY+1));
        tree.edge(0);
        assert(neuro_update_edge_weight(0,2,static_cast<unsigned>(network->edges[0].weight))==0);
        auto destination=route_synaptic_signal(0,NEURO_ACTIVATION_THRESHOLD);
        assert(destination==1||destination==2);
        if(destination==2)tree.edge(0);
        check();
    }
    std::cout<<"incremental Merkle root equals complete rebuild through 25 transitions: PASS\n";
}
