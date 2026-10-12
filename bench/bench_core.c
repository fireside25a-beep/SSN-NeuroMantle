#define _POSIX_C_SOURCE 200809L
#include "neuro_mantle.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double seconds(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))exit(2);
    return (double)t.tv_sec+(double)t.tv_nsec/1e9;}
int main(int argc,char **argv){
    if(argc!=5){fprintf(stderr,"usage: bench_core NEURONS TICKS MODE REPEAT\n");return 2;}
    unsigned n=0,ticks=0,repeat=0;char mode=0;
    if(sscanf(argv[1],"%u",&n)!=1||sscanf(argv[2],"%u",&ticks)!=1||
       sscanf(argv[3],"%c",&mode)!=1||sscanf(argv[4],"%u",&repeat)!=1||
       n<2||n>NM_MAX_NEURONS||ticks<2||ticks>1000000||repeat<1||repeat>100||
       (mode!='q'&&mode!='d'))return 2;
    nm_network *net=malloc(sizeof(*net));if(!net)return 2;
    nm_init(net);
    for(unsigned i=0;i<n;++i)if(nm_add_neuron(net,(uint16_t)i,0,10,NM_Q16,0))return 3;
    for(unsigned i=0;i<n;++i)for(unsigned j=1;j<=4;++j)
        if(nm_add_edge(net,(uint16_t)i,(uint16_t)((i+j)%n),1,1,0,0,4,0,10))return 3;
    uint64_t total_spikes=0;double start=seconds();
    for(unsigned tick=0;tick<ticks;++tick){
        unsigned stride=mode=='d'?16:n;
        for(unsigned id=0;id<n;id+=stride)
            if(nm_inject(net,(uint16_t)id,tick,10))return 3;
        nm_spikes spikes={0};if(nm_step(net,&spikes))return 3;
        total_spikes+=spikes.count;
    }
    double elapsed=seconds()-start;
    printf("%u,%u,%c,%u,%.9f,%" PRIu64 ",%zu\n",n,ticks,mode,repeat,elapsed,total_spikes,net->edge_count);
    free(net);return 0;
}
