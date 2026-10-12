#include "neuro_mantle_ecc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void flip(uint8_t *word,unsigned bit){word[bit/8]^=(uint8_t)(1u<<(bit%8));}
int main(void){
    const uint64_t patterns[]={0,UINT64_MAX,UINT64_C(0x0123456789abcdef),UINT64_C(0x8000000000000001)};
    for(size_t p=0;p<sizeof(patterns)/sizeof(patterns[0]);++p){
        uint8_t word[9];nm_ecc_encode64(patterns[p],word);uint64_t value=0;
        assert(nm_ecc_decode64(word,&value)==0&&value==patterns[p]);
        for(unsigned i=0;i<72;++i){uint8_t corrupt[9];memcpy(corrupt,word,9);flip(corrupt,i);
            assert(nm_ecc_decode64(corrupt,&value)==1&&value==patterns[p]);
        }
        for(unsigned i=0;i<72;++i)for(unsigned j=i+1;j<72;++j){
            uint8_t corrupt[9];memcpy(corrupt,word,9);flip(corrupt,i);flip(corrupt,j);
            assert(nm_ecc_decode64(corrupt,&value)==-1);
        }
    }
    nm_network *original=calloc(1,sizeof(*original)),*restored=calloc(1,sizeof(*restored));
    size_t length=nm_ecc_checkpoint_size(),corrected=0;uint8_t *snapshot=malloc(length);
    assert(original&&restored&&snapshot);
    original->tick=91;original->neurons[7].voltage=-1234567;original->edges[2].weight=47;
    original->pending[4][19]=991;
    assert(nm_ecc_capture(original,snapshot,length)==0);
    memset(restored,0xa5,sizeof(*restored));
    flip(snapshot,17);
    assert(nm_ecc_restore(snapshot,length,restored,&corrected)==0&&corrected==1);
    assert(memcmp(original,restored,sizeof(*original))==0);
    flip(snapshot,18);
    restored->tick=123;corrected=999;
    assert(nm_ecc_restore(snapshot,length,restored,&corrected)==-1);
    assert(restored->tick==123&&corrected==999);
    free(snapshot);free(original);free(restored);
    puts("SECDED word exhaustive single/double faults and atomic checkpoint restore: PASS");
    return 0;
}
