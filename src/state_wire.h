#pragma once
#include "board_state.h"
#include <bit>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
namespace state_wire {
constexpr size_t max_file=64u*1024u*1024u;
struct Writer {
    std::vector<uint8_t> bytes;
    void append(std::span<const uint8_t> in){if(in.size()>max_file-bytes.size())throw std::runtime_error("state exceeds64MiB");bytes.insert(bytes.end(),in.begin(),in.end());}
    void byte(uint8_t b){if(bytes.size()>=max_file)throw std::runtime_error("state exceeds64MiB");bytes.push_back(b);}
};
struct Reader {
    std::span<const uint8_t> bytes;size_t pos=0;
    explicit Reader(std::span<const uint8_t> s):bytes(s){if(s.size()>max_file)throw std::runtime_error("state exceeds64MiB");}
    size_t remaining()const{return bytes.size()-pos;}
    std::span<const uint8_t> take(size_t n){if(n>remaining())throw std::runtime_error("truncated state");auto out=bytes.subspan(pos,n);pos+=n;return out;}
    uint8_t byte(){return take(1)[0];}
    void finish()const{if(remaining())throw std::runtime_error("trailing state bytes");}
};
template<class>struct is_array:std::false_type{};
template<class T,size_t N>struct is_array<std::array<T,N>>:std::true_type{};
template<class>struct is_vector:std::false_type{};
template<class T,class A>struct is_vector<std::vector<T,A>>:std::true_type{using element=T;};
template<class>struct is_optional:std::false_type{};
template<class T>struct is_optional<std::optional<T>>:std::true_type{};
template<class T>constexpr bool timer_event=requires(T t){t.clock;t.reg;t.value;};
template<class T>void put(Writer&,const T&);
template<class T>void get(Reader&,T&);
template<class T>constexpr size_t vector_limit(){
    if constexpr(std::is_same_v<T,uint8_t>)return max_file;
    else if constexpr(std::is_same_v<T,std::array<int32_t,2>>)return 4000000;
    else if constexpr(std::is_same_v<T,PrimitiveRegistry::Block>)return 128;
    else if constexpr(timer_event<T>)return 256;
    else{static_assert(sizeof(T)==0,"unsupported dynamic state type");}
}
template<class T>constexpr size_t vector_min_wire(){
    if constexpr(std::is_same_v<T,uint8_t>)return 1;
    else if constexpr(std::is_same_v<T,std::array<int32_t,2>>)return 8;
    else if constexpr(std::is_same_v<T,PrimitiveRegistry::Block>)return 8;
    else return 10;
}
template<class T>void put(Writer& w,const T& value){
    if constexpr(std::is_same_v<T,bool>)w.byte(value?1:0);
    else if constexpr(std::is_integral_v<T>){using U=std::make_unsigned_t<T>;U bits=static_cast<U>(value);for(size_t i=0;i<sizeof(T);++i)w.byte(uint8_t(bits>>(8*i)));}
    else if constexpr(is_array<T>::value){for(const auto& v:value)put(w,v);}
    else if constexpr(std::is_same_v<T,std::string>){
        if(value.empty()||value.size()>128)throw std::runtime_error("invalid state field name size");
        put(w,uint32_t(value.size()));for(unsigned char c:value){if(c<32||c>126)throw std::runtime_error("invalid state field ASCII");w.byte(c);}
    }else if constexpr(is_vector<T>::value){
        using E=typename is_vector<T>::element;if(value.size()>vector_limit<E>())throw std::runtime_error("state vector exceeds bound");put(w,uint32_t(value.size()));
        if constexpr(std::is_same_v<E,uint8_t>)w.append(value);else for(const auto& v:value)put(w,v);
    }else if constexpr(is_optional<T>::value){put(w,value.has_value());if(value)put(w,*value);}
    else if constexpr(std::is_same_v<T,PrimitiveRegistry::Block>){put(w,value.name);put(w,value.bytes);}
    else if constexpr(std::is_same_v<T,SunsetBoardSnapshot>){put(w,value.schema);put(w,value.rom_identity);put(w,value.fields);put(w,value.unmapped_writes);put(w,value.checksum);}
    else if constexpr(timer_event<T>){put(w,value.clock);put(w,value.reg);put(w,value.value);}
    else if constexpr(requires{typename T::mapped_type;}){
        if(value.size()>65536)throw std::runtime_error("too many state map entries");put(w,uint32_t(value.size()));for(const auto& [k,v]:value){put(w,k);put(w,v);}
    }else if constexpr(requires{std::tuple_size<T>::value;})std::apply([&](const auto&... v){(put(w,v),...);},value);
    else static_assert(sizeof(T)==0,"unsupported state field");
}
template<class T>void get(Reader& r,T& value){
    if constexpr(std::is_same_v<T,bool>){auto v=r.byte();if(v>1)throw std::runtime_error("noncanonical state bool");value=v!=0;}
    else if constexpr(std::is_integral_v<T>){using U=std::make_unsigned_t<T>;auto bytes=r.take(sizeof(T));U bits=0;for(size_t i=0;i<sizeof(T);++i)bits|=U(bytes[i])<<(8*i);if constexpr(std::is_signed_v<T>)value=std::bit_cast<T>(bits);else value=bits;}
    else if constexpr(is_array<T>::value){for(auto& v:value)get(r,v);}
    else if constexpr(std::is_same_v<T,std::string>){
        uint32_t n=0;get(r,n);if(!n||n>128)throw std::runtime_error("invalid state field name size");auto bytes=r.take(n);
        for(auto c:bytes)if(c<32||c>126)throw std::runtime_error("invalid state field ASCII");value.assign(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    }else if constexpr(is_vector<T>::value){
        using E=typename is_vector<T>::element;uint32_t n=0;get(r,n);
        if(n>vector_limit<E>()||n>r.remaining()/vector_min_wire<E>())throw std::runtime_error("oversized/truncated state vector");
        if constexpr(std::is_same_v<E,uint8_t>){auto bytes=r.take(n);value.assign(bytes.begin(),bytes.end());}
        else{value.resize(n);for(auto& v:value)get(r,v);}
    }else if constexpr(is_optional<T>::value){bool present=false;get(r,present);if(present){value.emplace();get(r,*value);}else value.reset();}
    else if constexpr(std::is_same_v<T,PrimitiveRegistry::Block>){get(r,value.name);get(r,value.bytes);}
    else if constexpr(std::is_same_v<T,SunsetBoardSnapshot>){get(r,value.schema);get(r,value.rom_identity);get(r,value.fields);get(r,value.unmapped_writes);get(r,value.checksum);if(value.schema!=1)throw std::runtime_error("unsupported board state schema");}
    else if constexpr(timer_event<T>){get(r,value.clock);get(r,value.reg);get(r,value.value);if(value.reg!=0x10&&value.reg!=0x11&&value.reg!=0x12&&value.reg!=0x14)throw std::runtime_error("invalid timer trace register");}
    else if constexpr(requires{typename T::mapped_type;}){
        uint32_t n=0;get(r,n);using K=typename T::key_type;using V=typename T::mapped_type;
        static_assert(std::is_integral_v<K>&&std::is_integral_v<V>);
        if(n>65536||n>r.remaining()/(sizeof(K)+sizeof(V)))throw std::runtime_error("oversized state map");
        value.clear();std::optional<K> previous;
        for(uint32_t i=0;i<n;++i){K k{};V v{};get(r,k);get(r,v);if(previous&&k<=*previous)throw std::runtime_error("unordered/duplicate state map key");value.emplace(k,v);previous=k;}
    }else if constexpr(requires{std::tuple_size<T>::value;})std::apply([&](auto&... v){(get(r,v),...);},value);
    else static_assert(sizeof(T)==0,"unsupported state field");
}
template<class T>std::vector<uint8_t> encode(const T& value){Writer w;put(w,value);return std::move(w.bytes);}
template<class T>T decode(std::span<const uint8_t> bytes){Reader r(bytes);T value{};get(r,value);r.finish();return value;}
static_assert(sizeof(int)==4&&sizeof(unsigned)==4,"state ABI requires32bit native int");
}
