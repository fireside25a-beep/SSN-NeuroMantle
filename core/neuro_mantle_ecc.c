#include "neuro_mantle_ecc.h"
#include <stdlib.h>
#include <string.h>

static unsigned bit(const uint8_t *bytes,unsigned position){
    unsigned index=position-1;return (bytes[index/8]>>(index%8))&1u;
}
static void toggle(uint8_t *bytes,unsigned position){unsigned index=position-1;bytes[index/8]^=(uint8_t)(1u<<(index%8));}
static int parity_position(unsigned i){return i==1||i==2||i==4||i==8||i==16||i==32||i==64;}
void nm_ecc_encode64(uint64_t value,uint8_t codeword[9]){
    if(!codeword)return;
    memset(codeword,0,9);unsigned data_bit=0;
    for(unsigned i=1;i<=71;++i)if(!parity_position(i)){
        if((value>>data_bit)&1u)toggle(codeword,i);
        ++data_bit;
    }
    for(unsigned p=1;p<=64;p*=2){unsigned parity=0;
        for(unsigned i=1;i<=71;++i)if(i&p)parity^=bit(codeword,i);
        if(parity)toggle(codeword,p);
    }
    unsigned overall=0;for(unsigned i=1;i<=71;++i)overall^=bit(codeword,i);
    if(overall)toggle(codeword,72);
}
int nm_ecc_decode64(const uint8_t codeword[9],uint64_t *value){
    if(!codeword||!value)return -1;
    unsigned syndrome=0,overall=0;
    for(unsigned p=1;p<=64;p*=2){unsigned parity=0;
        for(unsigned i=1;i<=71;++i)if(i&p)parity^=bit(codeword,i);
        if(parity)syndrome|=p;
    }
    for(unsigned i=1;i<=72;++i)overall^=bit(codeword,i);
    if(syndrome && (!overall||syndrome>71))return -1;
    uint8_t restored[9];memcpy(restored,codeword,9);
    int corrected=0;
    if(overall){toggle(restored,syndrome?syndrome:72);corrected=1;}
    uint64_t result=0;unsigned data_bit=0;
    for(unsigned i=1;i<=71;++i)if(!parity_position(i)){
        result|=(uint64_t)bit(restored,i)<<data_bit;++data_bit;
    }
    *value=result;return corrected;
}
size_t nm_ecc_checkpoint_size(void){return ((sizeof(nm_network)+7)/8)*9;}
int nm_ecc_capture(const nm_network *network,uint8_t *out,size_t length){
    if(!network||!out||length!=nm_ecc_checkpoint_size())return -1;
    const uint8_t *raw=(const uint8_t*)network;
    for(size_t at=0,word=0;at<sizeof(*network);at+=8,++word){uint64_t x=0;
        for(unsigned i=0;i<8&&at+i<sizeof(*network);++i)x|=(uint64_t)raw[at+i]<<(8*i);
        nm_ecc_encode64(x,out+9*word);
    }
    return 0;
}
int nm_ecc_restore(const uint8_t *data,size_t length,nm_network *network,size_t *corrected){
    if(!network||!data||!corrected||length!=nm_ecc_checkpoint_size())return -1;
    uint8_t *raw=malloc(sizeof(*network));if(!raw)return -1;
    size_t count=0;int result=-1;
    for(size_t at=0,word=0;at<sizeof(*network);at+=8,++word){uint64_t x=0;
        int status=nm_ecc_decode64(data+9*word,&x);
        if(status<0)goto done;
        count+=(size_t)status;
        for(unsigned i=0;i<8&&at+i<sizeof(*network);++i)raw[at+i]=(uint8_t)(x>>(8*i));
    }
    memcpy(network,raw,sizeof(*network));*corrected=count;result=0;
done:
    free(raw);return result;
}
