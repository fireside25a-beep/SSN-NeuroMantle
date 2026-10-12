#include "neuro_mantle.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int add_i64(int64_t a, int64_t b, int64_t *out) {
    return !__builtin_add_overflow(a, b, out);
}
void nm_init(nm_network *n) { if (n) memset(n, 0, sizeof(*n)); }
int nm_add_neuron(nm_network *n, uint16_t id, int64_t reset, int64_t threshold,
                  uint32_t leak, uint32_t refractory) {
    if (!n || id >= NM_MAX_NEURONS || n->neurons[id].configured || leak > NM_Q16 ||
        threshold <= reset || refractory > NM_MAX_DELAY || reset < -(1LL<<45) || threshold > (1LL<<45)) return -1;
    n->neurons[id] = (nm_neuron){.voltage=reset,.reset=reset,.threshold=threshold,
        .leak_q16=leak,.refractory=refractory,.configured=1};
    return 0;
}
int nm_add_edge(nm_network *n,uint16_t src,uint16_t dst,int32_t weight,
                uint16_t delay,uint16_t plus,uint16_t minus,uint16_t window,
                int32_t min_weight,int32_t max_weight) {
    if (!n || src >= NM_MAX_NEURONS || dst >= NM_MAX_NEURONS ||
        !n->neurons[src].configured || !n->neurons[dst].configured ||
        !delay || delay > NM_MAX_DELAY || !window || window > 4096 ||
        min_weight > max_weight || weight < min_weight || weight > max_weight ||
        n->edge_count >= NM_MAX_EDGES) return -1;
    n->edges[n->edge_count++] = (nm_edge){src,dst,delay,weight,weight,min_weight,max_weight,plus,minus,window};
    return 0;
}
int nm_inject(nm_network *n,uint16_t id,uint64_t tick,int64_t current) {
    int64_t value;
    if (!n || id >= NM_MAX_NEURONS || !n->neurons[id].configured || tick < n->tick ||
        tick - n->tick > NM_MAX_DELAY || !add_i64(n->pending[tick % (NM_MAX_DELAY+1)][id],current,&value)) return -1;
    n->pending[tick % (NM_MAX_DELAY+1)][id]=value;
    return 0;
}
static int32_t clamp_weight(int64_t value, const nm_edge *e) {
    if (value < e->min_weight) return e->min_weight;
    if (value > e->max_weight) return e->max_weight;
    return (int32_t)value;
}
typedef struct {uint32_t key,order;int32_t weight;} nm_delivery;
static int compare_delivery(const void *a,const void *b){
    const nm_delivery *x=a,*y=b;
    if(x->key!=y->key)return x->key<y->key?-1:1;
    return x->order<y->order?-1:x->order>y->order;
}
int nm_step(nm_network *n,nm_spikes *out) {
    if (!n || !out || n->tick == UINT64_MAX) return -1;
    /* Validate all writes before committing. No full-network copy or rollback is needed. */
    nm_neuron next[NM_MAX_NEURONS];
    int32_t weights[NM_MAX_EDGES];
    nm_delivery deliveries[NM_MAX_EDGES];
    size_t delivery_count=0;
    unsigned char fired[NM_MAX_NEURONS]={0};
    nm_spikes result={.count=0,.tick=n->tick};
    const size_t slot=n->tick % (NM_MAX_DELAY+1);
    for (size_t id=0;id<NM_MAX_NEURONS;++id) {
        if (!n->neurons[id].configured) continue;
        next[id]=n->neurons[id];nm_neuron *v=&next[id];
        if (v->remaining) { --v->remaining; v->voltage=v->reset; continue; }
        int64_t delta,leaked,voltage;
        if (__builtin_sub_overflow(v->voltage,v->reset,&delta)) return -1;
        if (__builtin_mul_overflow(delta,(int64_t)v->leak_q16,&leaked)) return -1;
        leaked /= NM_Q16;
        if (!add_i64(v->reset,leaked,&voltage) ||
            !add_i64(voltage,n->pending[slot][id],&voltage)) return -1;
        if (voltage >= v->threshold) {
            if (v->spikes==UINT64_MAX) return -1;
            v->voltage=v->reset; v->remaining=v->refractory;
            v->last_spike=n->tick+1; ++v->spikes;
            fired[id]=1;result.ids[result.count++]=(uint16_t)id;
        } else v->voltage=voltage;
    }
    /* All pre/post events use the old timestamps. Simultaneous spikes do not train. */
    for (size_t i=0;i<n->edge_count;++i) {
        const nm_edge *e=&n->edges[i];
        const nm_neuron *pre=&n->neurons[e->src], *post=&n->neurons[e->dst];
        int64_t w=e->weight;
        if (fired[e->src]) {
            if (post->last_spike && n->tick+1-post->last_spike<=e->window)
                w-= (int64_t)e->minus*(e->window-(n->tick+1-post->last_spike)+1)/e->window;
        }
        if (fired[e->dst] && pre->last_spike && n->tick+1-pre->last_spike<=e->window)
            w+= (int64_t)e->plus*(e->window-(n->tick+1-pre->last_spike)+1)/e->window;
        weights[i]=clamp_weight(w,e);
    }
    /* Gather only firing-edge deliveries; preserve edge order within each destination. */
    for (size_t i=0;i<n->edge_count;++i) {
        const nm_edge *e=&n->edges[i];
        if (!fired[e->src]) continue;
        const size_t target=(slot+e->delay)%(NM_MAX_DELAY+1);
        deliveries[delivery_count++]=(nm_delivery){(uint32_t)(target*NM_MAX_NEURONS+e->dst),(uint32_t)i,e->weight};
    }
    qsort(deliveries,delivery_count,sizeof(*deliveries),compare_delivery);
    uint32_t last_key=UINT32_MAX;int64_t value=0;
    for(size_t i=0;i<delivery_count;++i){
        const uint32_t key=deliveries[i].key;
        if(key!=last_key){last_key=key;value=n->pending[key/NM_MAX_NEURONS][key%NM_MAX_NEURONS];}
        if(!add_i64(value,deliveries[i].weight,&value))return -1;
    }
    for(size_t id=0;id<NM_MAX_NEURONS;++id)if(n->neurons[id].configured)n->neurons[id]=next[id];
    for(size_t i=0;i<n->edge_count;++i)n->edges[i].weight=weights[i];
    memset(n->pending[slot],0,sizeof(n->pending[slot]));
    last_key=UINT32_MAX;
    for(size_t i=0;i<delivery_count;++i){
        const uint32_t key=deliveries[i].key;
        if(key!=last_key){last_key=key;value=n->pending[key/NM_MAX_NEURONS][key%NM_MAX_NEURONS];}
        value+=deliveries[i].weight;
        if(i+1==delivery_count||deliveries[i+1].key!=key)
            n->pending[key/NM_MAX_NEURONS][key%NM_MAX_NEURONS]=value;
    }
    ++n->tick;
    *out=result;
    return 0;
}
