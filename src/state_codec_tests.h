#pragma once
#include "state_file.h"
inline void state_codec_tests(const std::filesystem::path& prefix){
    using namespace state_wire;using state_file::Bytes;unsigned rejected=0,passed=0;
    auto require=[&](bool ok,const char* why){if(!ok)throw std::runtime_error(why);++passed;};
    auto reject=[&](auto fn){bool bad=false;try{fn();}catch(const std::exception&){bad=true;}if(!bad)throw std::runtime_error("invalid codec fixture accepted");++rejected;};
    require(state_file::hex(state_file::sha256({}))=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","SHA256empty fixture");
    const std::array<uint8_t,3> abc={'a','b','c'};
    require(state_file::hex(state_file::sha256(abc))=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256abc fixture");
    auto typed=std::make_tuple(int32_t(-2147483647-1),uint64_t(-1),true,std::array<uint16_t,2>{0x1234,0xabcd});
    require(decode<decltype(typed)>(encode(typed))==typed,"typed wire roundtrip");
    std::array<Bytes,5> sections;for(unsigned i=0;i<5;++i)sections[i]={uint8_t(i),42};
    auto valid=state_file::envelope(sections);auto decoded=state_file::unpack(valid);
    for(unsigned i=0;i<5;++i)require(Bytes(decoded[i].begin(),decoded[i].end())==sections[i],"section roundtrip");
    auto refresh=[](Bytes& bytes){uint64_t n=bytes.size()-state_file::header_size;for(unsigned i=0;i<8;++i)bytes[112+i]=uint8_t(n>>(8*i));auto h=state_file::sha256(std::span<const uint8_t>(bytes).subspan(state_file::header_size));std::copy(h.begin(),h.end(),bytes.begin()+120);};
    for(unsigned failure=0;failure<13;++failure){auto bad=valid;
        if(failure==0)bad[0]^=1;
        if(failure==1)bad[8]=2;
        if(failure==2)bad[12]=0;
        if(failure==3)bad[16]=bad[16]=='0'?'1':'0';
        if(failure==4)bad[80]^=1;
        if(failure==5)bad[112]^=1;
        if(failure==6)bad[120]^=1;
        if(failure==7){bad[152]='X';refresh(bad);}
        if(failure==8){std::copy(state_file::tags[0].begin(),state_file::tags[0].end(),bad.begin()+170);refresh(bad);}
        if(failure==9){bad[156]=2;refresh(bad);}
        if(failure==10){for(unsigned i=0;i<8;++i)bad[160+i]=0xff;refresh(bad);}
        if(failure==11){bad.push_back(0);refresh(bad);}
        if(failure==12){bad.resize(bad.size()-18);refresh(bad);}
        reject([&](){state_file::unpack(bad);});
    }
    for(size_t n:{0u,1u,7u,8u,12u,16u,80u,120u,151u})reject([&](){state_file::unpack(std::span<const uint8_t>(valid).first(n));});
    reject([](){decode<bool>(Bytes{2});});reject([](){decode<uint64_t>(Bytes(7));});
    auto count=encode(uint32_t(-1));
    reject([&](){decode<Bytes>(count);});reject([&](){decode<std::vector<std::array<int32_t,2>>>(count);});reject([&](){decode<PrimitiveRegistry::Snapshot>(count);});
    reject([](){Writer w;put(w,uint32_t(129));decode<std::string>(w.bytes);});
    reject([](){Writer w;put(w,uint32_t(1));w.byte(0);decode<std::string>(w.bytes);});
    reject([](){Writer w;put(w,uint32_t(2));put(w,uint32_t(1));put(w,uint64_t(4));put(w,uint32_t(1));put(w,uint64_t(5));decode<std::map<uint32_t,uint64_t>>(w.bytes);});
    reject([&](){auto bad=encode(typed);bad.push_back(0);decode<decltype(typed)>(bad);});
    reject([](){Bytes large(max_file+1);Reader r(large);});
    auto path=prefix;path+=L".ssrstate";state_file::write_atomic(path,valid);
    auto newer_sections=sections;newer_sections[0][0]=88;auto newer=state_file::envelope(newer_sections);
    reject([&](){state_file::write_atomic(path,newer,true);});
    require(state_file::read(path)==valid,"interrupted write changed existing state");
    state_file::write_atomic(path,newer);auto backup=path;backup+=L".bak";
    require(state_file::read(path)==newer&&state_file::read(backup)==valid,"atomic replacement/backup fixture");
    auto temporary_prefix=path.filename().wstring()+L".tmp-";
    for(const auto& entry:std::filesystem::directory_iterator(path.parent_path()))require(entry.path().filename().wstring().find(temporary_prefix)!=0,"owned temporary leaked after write");
    std::cout<<"{\"status\":\"passed\",\"scope\":\"bounded_state_codec_and_atomic_io\",\"positive_checks\":"<<passed<<",\"invalid_rejections\":"<<rejected<<"}\n";
}
