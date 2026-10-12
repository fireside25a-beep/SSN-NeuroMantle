#pragma once
#include <algorithm>
#include <compare>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace surreal {
class BigInt {
    std::vector<std::uint32_t> limb_;
    bool negative_=false;
    void trim(){while(!limb_.empty()&&!limb_.back())limb_.pop_back();if(limb_.empty())negative_=false;}
    static int magnitude(const BigInt&a,const BigInt&b){
        if(a.limb_.size()!=b.limb_.size())return a.limb_.size()<b.limb_.size()?-1:1;
        for(size_t i=a.limb_.size();i;--i)if(a.limb_[i-1]!=b.limb_[i-1])return a.limb_[i-1]<b.limb_[i-1]?-1:1;
        return 0;
    }
    static BigInt add_abs(const BigInt&a,const BigInt&b){BigInt c;std::uint64_t carry=0;
        for(size_t i=0;i<std::max(a.limb_.size(),b.limb_.size())||carry;++i){
            std::uint64_t x=carry+(i<a.limb_.size()?a.limb_[i]:0)+(i<b.limb_.size()?b.limb_[i]:0);
            c.limb_.push_back(static_cast<std::uint32_t>(x));carry=x>>32;}
        return c;}
    static BigInt sub_abs(const BigInt&a,const BigInt&b){BigInt c;std::uint64_t borrow=0;
        for(size_t i=0;i<a.limb_.size();++i){std::uint64_t x=a.limb_[i],y=(i<b.limb_.size()?b.limb_[i]:0)+borrow;
            c.limb_.push_back(static_cast<std::uint32_t>(x-y));borrow=x<y;}c.trim();return c;}
    bool bit(size_t n)const{return n/32<limb_.size() && ((limb_[n/32]>>(n%32))&1u);}
    void bit_set(size_t n){if(limb_.size()<=n/32)limb_.resize(n/32+1);limb_[n/32]|=std::uint32_t(1)<<(n%32);}
    static std::pair<BigInt,BigInt> divmod(BigInt a,BigInt b){
        if(!b)throw std::domain_error("division by zero");
        bool qneg=a.negative_!=b.negative_,rneg=a.negative_;
        a.negative_=b.negative_=false;BigInt q,r;
        for(size_t i=a.bits();i;--i){r<<=1;if(a.bit(i-1))r+=1;
            if(magnitude(r,b)>=0){r=sub_abs(r,b);q.bit_set(i-1);}}
        q.negative_=qneg;r.negative_=rneg;q.trim();r.trim();return {q,r};
    }
public:
    BigInt()=default;
    BigInt(long long n){std::uint64_t v=n<0?std::uint64_t(-(n+1))+1:std::uint64_t(n);
        negative_=n<0;if(v)limb_.push_back(static_cast<std::uint32_t>(v));if(v>>32)limb_.push_back(static_cast<std::uint32_t>(v>>32));}
    explicit BigInt(std::string_view s){if(s.empty())throw std::invalid_argument("empty integer");size_t i=0;
        bool neg=s[0]=='-';if(neg)i=1;if(i==s.size())throw std::invalid_argument("empty integer");
        for(;i<s.size();++i){if(s[i]<'0'||s[i]>'9')throw std::invalid_argument("invalid integer");*this*=10;*this+=BigInt(s[i]-'0');}
        if(*this)negative_=neg;}
    explicit operator bool()const{return !limb_.empty();}
    size_t bits()const{if(limb_.empty())return 0;auto x=limb_.back();size_t n=32*(limb_.size()-1);
        while(x){++n;x>>=1;}return n;}
    BigInt operator-()const{BigInt x=*this;if(x)x.negative_=!x.negative_;return x;}
    friend bool operator==(const BigInt&,const BigInt&)=default;
    friend std::strong_ordering operator<=>(const BigInt&a,const BigInt&b){
        if(a.negative_!=b.negative_)return a.negative_?std::strong_ordering::less:std::strong_ordering::greater;
        int c=magnitude(a,b);if(a.negative_)c=-c;
        return c<0?std::strong_ordering::less:c>0?std::strong_ordering::greater:std::strong_ordering::equal;}
    friend BigInt operator+(const BigInt&a,const BigInt&b){BigInt c;
        if(a.negative_==b.negative_){c=add_abs(a,b);c.negative_=a.negative_;}
        else {int comp=magnitude(a,b);c=comp>=0?sub_abs(a,b):sub_abs(b,a);c.negative_=comp>=0?a.negative_:b.negative_;}c.trim();return c;}
    friend BigInt operator-(const BigInt&a,const BigInt&b){return a+(-b);}
    friend BigInt operator*(const BigInt&a,const BigInt&b){BigInt c;if(!a||!b)return c;
        c.limb_.resize(a.limb_.size()+b.limb_.size());
        for(size_t i=0;i<a.limb_.size();++i){std::uint64_t carry=0;
            for(size_t j=0;j<b.limb_.size();++j){std::uint64_t x=std::uint64_t(a.limb_[i])*b.limb_[j]+c.limb_[i+j]+carry;
                c.limb_[i+j]=static_cast<std::uint32_t>(x);carry=x>>32;}
            size_t k=i+b.limb_.size();while(carry){std::uint64_t x=std::uint64_t(c.limb_[k])+carry;c.limb_[k]=static_cast<std::uint32_t>(x);carry=x>>32;++k;}}
        c.negative_=a.negative_!=b.negative_;c.trim();return c;}
    friend BigInt operator/(const BigInt&a,const BigInt&b){return divmod(a,b).first;}
    friend BigInt operator%(const BigInt&a,const BigInt&b){return divmod(a,b).second;}
    BigInt& operator+=(const BigInt&b){return *this=*this+b;}
    BigInt& operator*=(const BigInt&b){return *this=*this*b;}
    BigInt& operator/=(const BigInt&b){return *this=*this/b;}
    BigInt& operator++(){return *this+=1;}
    BigInt operator<<(size_t count)const{BigInt r;if(!*this)return r;size_t words=count/32,shift=count%32;
        r.limb_.resize(words+limb_.size()+1);std::uint64_t carry=0;
        for(size_t i=0;i<limb_.size();++i){auto x=(std::uint64_t(limb_[i])<<shift)|carry;r.limb_[i+words]=static_cast<std::uint32_t>(x);carry=x>>32;}
        r.limb_[words+limb_.size()]=static_cast<std::uint32_t>(carry);r.negative_=negative_;r.trim();return r;}
    BigInt& operator<<=(size_t count){return *this=*this<<count;}
    template<class T> T convert_to()const;
};
template<> inline std::string BigInt::convert_to<std::string>()const{
    if(!*this)return "0";
    BigInt value=*this;value.negative_=false;std::string s;
    while(value){auto [q,r]=divmod(value,BigInt(10));s.push_back(char('0'+(r.limb_.empty()?0:r.limb_[0])));value=std::move(q);}
    if(negative_)s.push_back('-');
    std::reverse(s.begin(),s.end());return s;
}
inline unsigned msb(const BigInt&n){if(!n)throw std::domain_error("msb zero");return static_cast<unsigned>(n.bits()-1);}
}
