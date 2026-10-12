#include "surreal/bigint.hpp"
#include <iostream>
#include <string>
int main(){std::string a,b;while(std::cin>>a>>b){
    try {surreal::BigInt x(a),y(b);
        std::cout<<(x+y).convert_to<std::string>()<<' '<<(x-y).convert_to<std::string>()<<' '
                 <<(x*y).convert_to<std::string>()<<' '<<(x/y).convert_to<std::string>()<<' '
                 <<(x%y).convert_to<std::string>()<<'\n';}
    catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}}
