#include "neuro_mantle.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void){
    nm_network *n=malloc(sizeof(*n));assert(n);nm_init(n);
    assert(nm_add_neuron(n,0,0,10,65536,0)==0);
    assert(nm_add_neuron(n,1,0,10,65536,0)==0);
    assert(nm_add_edge(n,0,1,5,1,4,3,4,0,20)==0);
    assert(nm_inject(n,0,0,10)==0);
    nm_spikes out;assert(nm_step(n,&out)==0&&out.count==1&&out.ids[0]==0);
    assert(nm_inject(n,1,1,5)==0);
    assert(nm_step(n,&out)==0&&out.count==1&&out.ids[0]==1);
    assert(n->edges[0].weight==9);
    assert(nm_inject(n,0,2,10)==0);
    assert(nm_step(n,&out)==0&&out.count==1&&out.ids[0]==0);
    assert(n->edges[0].weight==6); /* recent post spike depresses */
    assert(nm_add_neuron(n,0,0,10,65536,0)!=0);
    assert(nm_add_edge(n,0,1,1,0,0,0,4,0,20)!=0);
    assert(nm_inject(n,0,n->tick+65,1)!=0);
    n->neurons[0].voltage=1;
    assert(nm_inject(n,0,n->tick,INT64_MAX)==0);
    unsigned long long old_tick=(unsigned long long)n->tick;
    assert(nm_step(n,&out)!=0&&n->tick==old_tick);
    assert(n->edges[0].weight==6);
    nm_network *delivery=malloc(sizeof(*delivery)),*snapshot=malloc(sizeof(*snapshot));
    assert(delivery&&snapshot);nm_init(delivery);
    assert(nm_add_neuron(delivery,0,0,10,65536,0)==0);
    assert(nm_add_neuron(delivery,1,0,10,65536,0)==0);
    assert(nm_add_edge(delivery,0,1,1,1,0,0,1,0,1)==0);
    assert(nm_inject(delivery,0,0,10)==0);
    assert(nm_inject(delivery,1,1,INT64_MAX)==0);
    memcpy(snapshot,delivery,sizeof(*delivery));
    assert(nm_step(delivery,&out)!=0);
    assert(memcmp(snapshot,delivery,sizeof(*delivery))==0);
    free(snapshot);free(delivery);
    free(n);puts("core dynamics, delayed delivery, LTP, LTD, refusal, transactional overflow: PASS");
}
