/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "advanced_tools_internal.hpp"

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace calculator::tools::detail {

// Exact rational and arbitrary-precision integer arithmetic are isolated here so their bespoke numeric representations do not leak into the general tool dispatcher.
class BigInt {
public:
    BigInt()=default;
    explicit BigInt(std::int64_t value){
        std::uint64_t magnitude=0U;
        if(value<0){
            negative_=true;
            magnitude=
                static_cast<std::uint64_t>(-(value+1)) + 1U;
        } else {
            magnitude=static_cast<std::uint64_t>(value);
        }
        while(magnitude){
            limbs_.push_back(
                static_cast<std::uint32_t>(magnitude%kBase));
            magnitude/=kBase;
        }
    }
    static bool parse(std::string_view digits,BigInt& out) {
        if(digits.empty()) return false;
        out=BigInt();
        for(char ch:digits){
            if(ch<'0'||ch>'9') return false;
            out.mul_small(10U); out.add_small(static_cast<unsigned>(ch-'0'));
            if(out.decimal_digits()>20000U) return false;
        }
        return true;
    }
    bool zero() const{return limbs_.empty();}
    bool negative() const{return negative_&&!zero();}
    void negate(){if(!zero())negative_=!negative_;}
    BigInt absolute() const{BigInt x=*this;x.negative_=false;return x;}
    std::string str() const{
        if(zero())return"0";
        std::ostringstream out;if(negative())out<<'-';
        out<<limbs_.back();
        for(std::size_t i=limbs_.size()-1;i>0;--i)out<<std::setfill('0')<<std::setw(9)<<limbs_[i-1];
        return out.str();
    }
    std::size_t decimal_digits() const {
        if(zero())return 1U;
        std::size_t n=(limbs_.size()-1U)*9U;std::uint32_t top=limbs_.back();
        do{++n;top/=10U;}while(top); return n;
    }
    bool divisible_by_10() const {
        return divisible_by_small(10U);
    }
    bool divisible_by_small(std::uint32_t divisor) const {
        if (divisor == 0U || zero()) return false;
        std::uint64_t remainder = 0U;
        for (std::size_t i = limbs_.size(); i > 0; --i) {
            remainder =
                (remainder * kBase + limbs_[i - 1U]) % divisor;
        }
        return remainder == 0U;
    }
    void divide_small(std::uint32_t divisor) {
        if (divisor == 0U) return;
        std::uint64_t remainder = 0U;
        for (std::size_t i = limbs_.size(); i > 0; --i) {
            const std::uint64_t current =
                remainder * kBase + limbs_[i - 1U];
            limbs_[i - 1U] =
                static_cast<std::uint32_t>(current / divisor);
            remainder = current % divisor;
        }
        trim();
    }
    bool to_u32(std::uint32_t& out) const {
        if(negative()||limbs_.size()>2U)return false;
        std::uint64_t v=0;for(std::size_t i=limbs_.size();i>0;--i)v=v*kBase+limbs_[i-1];
        if (v > std::numeric_limits<std::uint32_t>::max()) return false;
        out = static_cast<std::uint32_t>(v);
        return true;
    }
    friend BigInt operator+(const BigInt&a,const BigInt&b){
        if(a.negative_==b.negative_){BigInt r=add_abs(a,b);r.negative_=a.negative_;return r;}
        const int c=cmp_abs(a,b);if(c==0)return BigInt();
        if(c>0){BigInt r=sub_abs(a,b);r.negative_=a.negative_;return r;}
        BigInt r=sub_abs(b,a);r.negative_=b.negative_;return r;
    }
    friend BigInt operator-(BigInt a,const BigInt&b){BigInt n=b;n.negate();return a+n;}
    friend BigInt operator*(const BigInt&a,const BigInt&b){
        if(a.zero()||b.zero())return BigInt();
        BigInt r;r.limbs_.assign(a.limbs_.size()+b.limbs_.size(),0U);
        for(std::size_t i=0;i<a.limbs_.size();++i){
            std::uint64_t carry=0;
            for(std::size_t j=0;j<b.limbs_.size()||carry;++j){
                const std::uint64_t cur=r.limbs_[i+j]+carry+
                    (j<b.limbs_.size()?static_cast<std::uint64_t>(a.limbs_[i])*b.limbs_[j]:0U);
                r.limbs_[i+j]=static_cast<std::uint32_t>(cur%kBase);carry=cur/kBase;
            }
        }
        r.negative_=a.negative_!=b.negative_;r.trim();return r;
    }
    static BigInt pow(BigInt base,std::uint32_t exp,bool& ok){
        BigInt result(1);ok=true;
        while(exp){
            if(exp&1U){result=result*base;if(result.decimal_digits()>20000U){ok=false;return {};}}
            exp>>=1U;if(exp){base=base*base;if(base.decimal_digits()>20000U){ok=false;return {};}}
        }return result;
    }
    static BigInt gcd(BigInt a, BigInt b) {
        a.negative_ = false;
        b.negative_ = false;
        if (a.zero()) return b;
        if (b.zero()) return a;

        std::uint32_t common_twos = 0U;
        while (a.divisible_by_small(2U) &&
               b.divisible_by_small(2U)) {
            a.divide_small(2U);
            b.divide_small(2U);
            ++common_twos;
        }
        while (a.divisible_by_small(2U)) a.divide_small(2U);

        do {
            while (b.divisible_by_small(2U)) b.divide_small(2U);
            if (cmp_abs(a, b) > 0) std::swap(a, b);
            b = sub_abs(b, a);
        } while (!b.zero());

        if (common_twos != 0U) {
            bool ok = false;
            const BigInt factor = pow(BigInt(2), common_twos, ok);
            if (ok) a = a * factor;
        }
        return a;
    }
    static bool divide_exact(
        const BigInt& value, const BigInt& divisor, BigInt& quotient) {
        if (divisor.zero()) return false;

        const bool negative = value.negative() != divisor.negative();
        const BigInt dividend = value.absolute();
        const BigInt divisor_abs = divisor.absolute();
        BigInt remainder;
        quotient = BigInt();
        quotient.limbs_.assign(dividend.limbs_.size(), 0U);

        for (std::size_t i = dividend.limbs_.size(); i > 0; --i) {
            remainder.limbs_.insert(
                remainder.limbs_.begin(), dividend.limbs_[i - 1U]);
            remainder.trim();

            std::uint32_t low = 0U;
            std::uint32_t high =
                static_cast<std::uint32_t>(kBase - 1U);
            std::uint32_t digit = 0U;
            while (low <= high) {
                const std::uint32_t middle =
                    low + static_cast<std::uint32_t>(
                        (static_cast<std::uint64_t>(high) - low) / 2U);
                BigInt product = divisor_abs;
                product.mul_small(middle);
                const int comparison = cmp_abs(product, remainder);
                if (comparison <= 0) {
                    digit = middle;
                    if (middle ==
                        static_cast<std::uint32_t>(kBase - 1U)) {
                        break;
                    }
                    low = middle + 1U;
                } else {
                    if (middle == 0U) break;
                    high = middle - 1U;
                }
            }

            if (digit != 0U) {
                BigInt product = divisor_abs;
                product.mul_small(digit);
                remainder = sub_abs(remainder, product);
            }
            quotient.limbs_[i - 1U] = digit;
        }

        quotient.trim();
        if (!remainder.zero()) {
            quotient = BigInt();
            return false;
        }
        quotient.negative_ = negative && !quotient.zero();
        return true;
    }
private:
    static constexpr std::uint64_t kBase=1000000000ULL;
    std::vector<std::uint32_t> limbs_;
    bool negative_=false;
    void trim(){while(!limbs_.empty()&&limbs_.back()==0U)limbs_.pop_back();if(limbs_.empty())negative_=false;}
    void mul_small(std::uint32_t m){std::uint64_t carry=0;for(auto&x:limbs_){const std::uint64_t v=static_cast<std::uint64_t>(x)*m+carry;x=static_cast<std::uint32_t>(v%kBase);carry=v/kBase;}if(carry)limbs_.push_back(static_cast<std::uint32_t>(carry));}
    void add_small(std::uint32_t a){std::uint64_t carry=a;std::size_t i=0;while(carry){if(i==limbs_.size())limbs_.push_back(0);const std::uint64_t v=limbs_[i]+carry;limbs_[i]=static_cast<std::uint32_t>(v%kBase);carry=v/kBase;++i;}}
    static int cmp_abs(const BigInt&a,const BigInt&b){if(a.limbs_.size()!=b.limbs_.size())return a.limbs_.size()<b.limbs_.size()?-1:1;for(std::size_t i=a.limbs_.size();i>0;--i)if(a.limbs_[i-1]!=b.limbs_[i-1])return a.limbs_[i-1]<b.limbs_[i-1]?-1:1;return 0;}
    static BigInt add_abs(const BigInt&a,const BigInt&b){BigInt r;const std::size_t n=std::max(a.limbs_.size(),b.limbs_.size());r.limbs_.resize(n);std::uint64_t carry=0;for(std::size_t i=0;i<n;++i){const std::uint64_t v=carry+(i<a.limbs_.size()?a.limbs_[i]:0U)+(i<b.limbs_.size()?b.limbs_[i]:0U);r.limbs_[i]=static_cast<std::uint32_t>(v%kBase);carry=v/kBase;}if(carry)r.limbs_.push_back(static_cast<std::uint32_t>(carry));return r;}
    static BigInt sub_abs(const BigInt&a,const BigInt&b){BigInt r;r.limbs_.resize(a.limbs_.size());std::int64_t borrow=0;for(std::size_t i=0;i<a.limbs_.size();++i){std::int64_t v=static_cast<std::int64_t>(a.limbs_[i])-borrow-(i<b.limbs_.size()?b.limbs_[i]:0U);if(v<0){v+=static_cast<std::int64_t>(kBase);borrow=1;}else borrow=0;r.limbs_[i]=static_cast<std::uint32_t>(v);}r.trim();return r;}
};

BigInt power10(std::size_t n,bool& ok){
    BigInt ten(10);return BigInt::pow(ten,static_cast<std::uint32_t>(n),ok);
}

struct Exact {BigInt n;BigInt d=BigInt(1);};

bool reduce_exact(Exact& value) {
    if (value.n.zero()) {
        value.d = BigInt(1);
        return true;
    }

    const BigInt divisor =
        BigInt::gcd(value.n.absolute(), value.d.absolute());
    BigInt numerator;
    BigInt denominator;
    if (!BigInt::divide_exact(value.n, divisor, numerator) ||
        !BigInt::divide_exact(value.d, divisor, denominator)) {
        return false;
    }
    if (denominator.negative()) {
        denominator.negate();
        numerator.negate();
    }
    value.n = std::move(numerator);
    value.d = std::move(denominator);
    return true;
}

bool terminating_decimal(const Exact& value, std::string& decimal) {
    BigInt denominator = value.d.absolute();
    std::size_t twos = 0U;
    std::size_t fives = 0U;
    while (denominator.divisible_by_small(2U)) {
        denominator.divide_small(2U);
        ++twos;
    }
    while (denominator.divisible_by_small(5U)) {
        denominator.divide_small(5U);
        ++fives;
    }
    if (denominator.str() != "1") return false;

    const std::size_t scale = std::max(twos, fives);
    // Keep presentation bounded independently of exact fraction support.
    if (scale > 20000U) return false;

    BigInt scaled = value.n.absolute();
    bool ok = true;
    if (twos < scale) {
        scaled = scaled * BigInt::pow(
            BigInt(2), static_cast<std::uint32_t>(scale - twos), ok);
        if (!ok) return false;
    }
    if (fives < scale) {
        scaled = scaled * BigInt::pow(
            BigInt(5), static_cast<std::uint32_t>(scale - fives), ok);
        if (!ok) return false;
    }

    std::string digits = scaled.str();
    if (scale != 0U) {
        if (digits.size() <= scale) {
            digits.insert(0, scale + 1U - digits.size(), '0');
        }
        digits.insert(digits.size() - scale, 1U, '.');
        while (digits.size() > 1U && digits.back() == '0') {
            digits.pop_back();
        }
        if (!digits.empty() && digits.back() == '.') {
            digits.pop_back();
        }
    }

    if (value.n.negative() && digits != "0") {
        digits.insert(digits.begin(), '-');
    }
    decimal = std::move(digits);
    return true;
}

class ExactParser {
public:
    explicit ExactParser(std::string_view in):in_(in){}
    ToolResult run(){
        Exact v=expr();skip();
        if(!error_.empty())return failure(error_);
        if(pos_!=in_.size())return failure("Unexpected input in exact-decimal expression.");
        if (!reduce_exact(v)) {
            return failure("Exact rational reduction failed.");
        }
        std::string frac=v.n.str()+"/"+v.d.str();
        std::string decimal;
        (void)terminating_decimal(v, decimal);
        std::string out="Exact fraction  "+frac;
        if(!decimal.empty())out+="\nExact decimal  "+decimal;
        return success(out);
    }
private:
    std::string_view in_;std::size_t pos_=0;std::string error_;
    void skip(){while(pos_<in_.size()&&infiltratr_ascii_is_space(static_cast<unsigned char>(in_[pos_])))++pos_;}
    bool take(char c){skip();if(pos_<in_.size()&&in_[pos_]==c){++pos_;return true;}return false;}
    Exact expr(){Exact a=term();while(error_.empty()){if(take('+')){Exact b=term();a={a.n*b.d+b.n*a.d,a.d*b.d};}else if(take('-')){Exact b=term();a={a.n*b.d-b.n*a.d,a.d*b.d};}else break;}return a;}
    Exact term(){Exact a=unary();while(error_.empty()){if(take('*')){Exact b=unary();a={a.n*b.n,a.d*b.d};}else if(take('/')){Exact b=unary();if(b.n.zero()){error_="Division by zero.";return{};}BigInt bn=b.n.absolute();BigInt n=a.n*b.d;if(b.n.negative())n.negate();a={n,a.d*bn};}else break;if(a.n.decimal_digits()+a.d.decimal_digits()>20000U){error_="Exact result exceeds the 20,000-digit safety limit.";}}return a;}
    Exact unary(){if(take('+'))return unary();if(take('-')){Exact a=unary();a.n.negate();return a;}return primary();}
    Exact primary(){
        if(take('(')){Exact v=expr();if(!take(')')&&error_.empty())error_="Missing closing parenthesis.";return v;}
        skip();const std::size_t start=pos_;bool dot=false;std::size_t fractional=0;
        while(pos_<in_.size()){
            char c=in_[pos_];if(c>='0'&&c<='9'){if(dot)++fractional;++pos_;}
            else if(c=='.'&&!dot){dot=true;++pos_;}else break;
        }
        if(pos_==start){error_="Expected a decimal literal.";return{};}
        std::string token(in_.substr(start,pos_-start));token.erase(std::remove(token.begin(),token.end(),'.'),token.end());
        if(token.empty()){error_="Invalid decimal literal.";return{};}
        BigInt n;if(!BigInt::parse(token,n)){error_="Invalid or oversized decimal literal.";return{};}
        int exponent=0;
        if(pos_<in_.size()&&(in_[pos_]=='e'||in_[pos_]=='E')){
            ++pos_;bool neg=false;if(pos_<in_.size()&&(in_[pos_]=='+'||in_[pos_]=='-')){neg=in_[pos_]=='-';++pos_;}
            const std::size_t es=pos_;while(pos_<in_.size()&&infiltratr_ascii_is_digit(static_cast<unsigned char>(in_[pos_])))++pos_;
            if(es==pos_){error_="Malformed decimal exponent.";return{};}
            std::uint64_t parsed_exponent = 0;
            if(!parse_u64_range(
                   in_.substr(es,pos_-es),0U,4096U,parsed_exponent)){
                error_="Decimal exponent exceeds safety limit.";return{};
            }
            exponent=static_cast<int>(parsed_exponent);if(neg)exponent=-exponent;
        }
        long long scale=static_cast<long long>(fractional)-exponent;bool ok=true;
        if(scale>=0){BigInt d=power10(static_cast<std::size_t>(scale),ok);if(!ok){error_="Decimal scale exceeds safety limit.";return{};}return{n,d};}
        BigInt mul=power10(static_cast<std::size_t>(-scale),ok);if(!ok){error_="Decimal scale exceeds safety limit.";return{};}return{n*mul,BigInt(1)};
    }
};

class IntegerParser {
public:
    explicit IntegerParser(std::string_view in):in_(in){}
    ToolResult run(){BigInt v=expr();skip();if(!err_.empty())return failure(err_);if(pos_!=in_.size())return failure("Unexpected arbitrary-precision input.");return success(v.str());}
private:
    std::string_view in_;std::size_t pos_=0;std::string err_;
    void skip(){while(pos_<in_.size()&&infiltratr_ascii_is_space(static_cast<unsigned char>(in_[pos_])))++pos_;}
    bool take(char c){skip();if(pos_<in_.size()&&in_[pos_]==c){++pos_;return true;}return false;}
    BigInt expr(){BigInt a=term();while(err_.empty()){if(take('+'))a=a+term();else if(take('-'))a=a-term();else break;}return a;}
    BigInt term(){BigInt a=unary();while(err_.empty()&&take('*')){a=a*unary();if(a.decimal_digits()>20000U)err_="Result exceeds the 20,000-digit safety limit.";}return a;}
    BigInt unary(){if(take('+'))return unary();if(take('-')){BigInt v=unary();v.negate();return v;}return power();}
    BigInt power(){BigInt a=postfix();if(err_.empty()&&take('^')){BigInt e=unary();std::uint32_t exp=0;if(!e.to_u32(exp)||exp>100000U){err_="Exponent must be a non-negative integer <= 100000.";return{};}bool ok=false;a=BigInt::pow(a,exp,ok);if(!ok)err_="Result exceeds the 20,000-digit safety limit.";}return a;}
    BigInt postfix(){BigInt a=primary();while(err_.empty()&&take('!')){std::uint32_t n=0;if(!a.to_u32(n)||n>1000U){err_="Factorial requires an integer from 0 to 1000.";return{};}BigInt r(1);for(std::uint32_t i=2;i<=n;++i)r=r*BigInt(i);a=r;}return a;}
    BigInt primary(){if(take('(')){BigInt v=expr();if(!take(')')&&err_.empty())err_="Missing closing parenthesis.";return v;}skip();const std::size_t start=pos_;while(pos_<in_.size()&&infiltratr_ascii_is_digit(static_cast<unsigned char>(in_[pos_])))++pos_;if(start==pos_){err_="Expected an integer.";return{};}BigInt v;if(!BigInt::parse(in_.substr(start,pos_-start),v))err_="Invalid or oversized integer.";return v;}
};

ToolResult exact_decimal_tool(std::string_view input) {
    return ExactParser(input).run();
}

ToolResult arbitrary_precision_tool(std::string_view input) {
    return IntegerParser(input).run();
}

} // namespace calculator::tools::detail
