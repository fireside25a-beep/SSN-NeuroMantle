#include "neuro_mantle.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
    unsigned count,edges,steps;if(scanf("%u %u %u",&count,&edges,&steps)!=3||count>16||edges>32||steps>32)return 2;
    nm_network *n=malloc(sizeof(*n));if(!n)return 2;nm_init(n);
    for(unsigned i=0;i<count;++i){long long reset,threshold;unsigned leak,refr;
        if(scanf("%lld %lld %u %u",&reset,&threshold,&leak,&refr)!=4||
            nm_add_neuron(n,(uint16_t)i,reset,threshold,leak,refr))return 2;}
    for(unsigned i=0;i<edges;++i){unsigned a,b,d,plus,minus,window;int w,lo,hi;
        if(scanf("%u %u %d %u %u %u %u %d %d",&a,&b,&w,&d,&plus,&minus,&window,&lo,&hi)!=9||
            nm_add_edge(n,(uint16_t)a,(uint16_t)b,w,(uint16_t)d,(uint16_t)plus,(uint16_t)minus,(uint16_t)window,lo,hi))return 2;}
    for(unsigned t=0;t<steps;++t){for(unsigned i=0;i<count;++i){long long current;
            if(scanf("%lld",&current)!=1||nm_inject(n,(uint16_t)i,t,current))return 2;}
        nm_spikes out;if(nm_step(n,&out))return 3;
        printf("%zu",out.count);for(size_t j=0;j<out.count;++j)printf(" %u",out.ids[j]);
        printf(" | ");for(unsigned i=0;i<count;++i)printf("%lld %u %llu %llu ",
            (long long)n->neurons[i].voltage,n->neurons[i].remaining,
            (unsigned long long)n->neurons[i].spikes,(unsigned long long)n->neurons[i].last_spike);
        printf("| ");for(unsigned i=0;i<edges;++i)printf("%d ",n->edges[i].weight);puts("");
    }
    free(n);return 0;
}
