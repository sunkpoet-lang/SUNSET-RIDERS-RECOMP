#pragma once
#include <bit>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
// Registers host bindings; only named, typed primitive bytes enter snapshots.
// Binding callbacks/addresses are retained by the live instance, never stored.
class PrimitiveRegistry {
public:
    struct Block {std::string name;std::vector<uint8_t> bytes;bool operator==(const Block&)const=default;};
    using Snapshot=std::vector<Block>;
    PrimitiveRegistry()=default;
    PrimitiveRegistry(const PrimitiveRegistry&)=delete;
    PrimitiveRegistry& operator=(const PrimitiveRegistry&)=delete;
    template<class T>static constexpr size_t wire_size(){
        if constexpr(std::is_array_v<T>)return std::extent_v<T>*wire_size<std::remove_extent_t<T>>();
        else{static_assert(std::is_integral_v<T>);return std::is_same_v<T,bool>?1:sizeof(T);}
    }
    template<class T>static void encode(const T& value,std::vector<uint8_t>& out){
        if constexpr(std::is_array_v<T>){for(const auto& v:value)encode(v,out);}
        else if constexpr(std::is_same_v<T,bool>)out.push_back(value?1:0);
        else{using U=std::make_unsigned_t<T>;U bits=static_cast<U>(value);
            for(size_t i=0;i<sizeof(T);++i)out.push_back(uint8_t(bits>>(8*i)));}
    }
    template<class T>static void decode(T& value,const std::vector<uint8_t>& in,size_t& pos,bool apply){
        if constexpr(std::is_array_v<T>){for(auto& v:value)decode(v,in,pos,apply);}
        else if constexpr(std::is_same_v<T,bool>){
            if(pos>=in.size()||in[pos]>1)throw std::runtime_error("invalid canonical bool");
            bool v=in[pos++]!=0;if(apply)value=v;
        }else{using U=std::make_unsigned_t<T>;U bits=0;
            if(in.size()-pos<sizeof(T))throw std::runtime_error("truncated primitive");
            for(size_t i=0;i<sizeof(T);++i)bits|=U(in[pos++])<<(8*i);
            if(apply){if constexpr(std::is_signed_v<T>)value=std::bit_cast<T>(bits);else value=bits;}}
    }
    template<class T,class... Index>void add(T& value,const char* name,Index... index){
        std::string key=name;((key+="/"+std::to_string(index)),...);
        for(const auto& e:entries)if(e.name==key)throw std::runtime_error("duplicate state registration");
        entries.push_back({key,wire_size<T>(),[&value](){std::vector<uint8_t> out;out.reserve(wire_size<T>());encode(value,out);return out;},
            [&value](const auto& bytes,bool apply){size_t pos=0;decode(value,bytes,pos,apply);if(pos!=bytes.size())throw std::runtime_error("trailing primitive bytes");}});
    }
    template<class T>void add_span(T* data,size_t count,const char* name){
        entries.push_back({name,count*wire_size<T>(),[data,count](){std::vector<uint8_t> out;out.reserve(count*wire_size<T>());for(size_t i=0;i<count;++i)encode(data[i],out);return out;},
            [data,count](const auto& bytes,bool apply){size_t pos=0;for(size_t i=0;i<count;++i)decode(data[i],bytes,pos,apply);if(pos!=bytes.size())throw std::runtime_error("trailing span bytes");}});
    }
    template<class E>void add_enum(E& value,E maximum,const char* name){
        using I=std::underlying_type_t<E>;
        entries.push_back({name,sizeof(I),[&value](){std::vector<uint8_t> out;I v=static_cast<I>(value);encode(v,out);return out;},
            [&value,maximum](const auto& bytes,bool apply){I v=0;size_t pos=0;decode(v,bytes,pos,true);if(v<0||v>static_cast<I>(maximum))throw std::runtime_error("invalid enum state");if(apply)value=static_cast<E>(v);}});
    }
    Snapshot capture()const{Snapshot out;out.reserve(entries.size());for(const auto& e:entries)out.push_back({e.name,e.pack()});return out;}
    void validate(const Snapshot& s)const{
        if(s.size()!=entries.size())throw std::runtime_error("component field count mismatch");
        for(size_t i=0;i<s.size();++i){if(s[i].name!=entries[i].name||s[i].bytes.size()!=entries[i].size)throw std::runtime_error("component field schema mismatch");entries[i].unpack(s[i].bytes,false);}
    }
    void restore(const Snapshot& s){validate(s);for(size_t i=0;i<s.size();++i)entries[i].unpack(s[i].bytes,true);}
    static uint64_t checksum(const Snapshot& s){
        uint64_t h=14695981039346656037ull;auto b=[&](uint8_t v){h=(h^v)*1099511628211ull;};
        for(const auto& v:s){for(unsigned char c:v.name)b(c);b(0);uint64_t size=v.bytes.size();for(unsigned i=0;i<8;++i)b(uint8_t(size>>(8*i)));for(auto c:v.bytes)b(c);}return h;
    }
private:
    struct Entry {std::string name;size_t size;std::function<std::vector<uint8_t>()> pack;std::function<void(const std::vector<uint8_t>&,bool)> unpack;};
    std::vector<Entry> entries;
};
