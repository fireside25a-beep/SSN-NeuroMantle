#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){
    if(argc!=4)return 2;
    char *end;unsigned long long values[3];
    for(int i=0;i<3;++i){values[i]=strtoull(argv[i+1],&end,10);if(*end)return 2;}
    uint64_t drive=7+(values[0]%3+values[1]%3+values[2]%3)%3;
    if(printf("%llu\n",(unsigned long long)drive)<0)return 1;
    return 0;
}
