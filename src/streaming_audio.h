#include "sunset_session_profile.h"
#pragma once
#include <array>
#include <tuple>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
// Experimental component: no recording archive or native chip state.
class SunsetStreamingAudio {
public:
    using Stereo=std::array<int32_t,2>;
    struct Lane {uint64_t origin=0;uint32_t count=1;std::array<Stereo,8> samples{};};
    using Snapshot=std::tuple<uint64_t,uint64_t,uint32_t,std::array<Stereo,8>,uint64_t,uint32_t,std::array<Stereo,8>>;
    static constexpr uint64_t limit=sunset_session::max_audio;
    uint64_t next=0;Lane fm,pcm;
    static uint64_t index(uint64_t n){return n*3579545/(48000ull*64);}
    static bool available(const Lane&l,uint64_t i){return i>=l.origin&&i-l.origin+1<l.count;}
    bool ready()const{auto i=index(next);return next<limit&&available(fm,i)&&available(pcm,i);}
    void push(bool is_fm,Stereo s){auto&l=is_fm?fm:pcm;if(l.count==8)throw std::runtime_error("streaming audio producer skew");l.samples[l.count++]=s;}
    static int64_t interpolate(const Lane&l,uint64_t n,unsigned ch){
        constexpr uint64_t d=48000ull*64;uint64_t position=n*3579545,i=position/d,remainder=position%d;
        int64_t a=l.samples[i-l.origin][ch],b=l.samples[i-l.origin+1][ch];return a+(b-a)*int64_t(remainder)/int64_t(d);
    }
    static void prune(Lane&l,uint64_t i){
        while(l.count>1&&l.origin+1<=i){for(unsigned j=1;j<l.count;++j)l.samples[j-1]=l.samples[j];l.samples[--l.count]={0,0};++l.origin;}
    }
    Stereo pop(){
        if(!ready())throw std::runtime_error("streaming audio needs future sample");Stereo out;
        for(unsigned c=0;c<2;++c){auto v=(10*interpolate(fm,next,c)+7*interpolate(pcm,next,c))/10;out[c]=int32_t(std::clamp<int64_t>(v,-32768,32767));}
        ++next;auto i=index(next);prune(fm,i);prune(pcm,i);return out;
    }
    Snapshot snapshot()const{return {next,fm.origin,fm.count,fm.samples,pcm.origin,pcm.count,pcm.samples};}
    void restore(const Snapshot&s){
        auto n=std::get<0>(s);if(n>limit)throw std::runtime_error("invalid streaming output cursor");auto i=index(n);
        Lane a{std::get<1>(s),std::get<2>(s),std::get<3>(s)},b{std::get<4>(s),std::get<5>(s),std::get<6>(s)};
        auto valid=[&](const Lane&l){
            if(l.count<1||l.count>8||l.origin>i||l.origin+l.count<i)throw std::runtime_error("invalid streaming source window");
            for(unsigned j=l.count;j<8;++j)if(l.samples[j]!=Stereo{0,0})throw std::runtime_error("noncanonical streaming source tail");
        };valid(a);valid(b);next=n;fm=a;pcm=b;
    }
};
