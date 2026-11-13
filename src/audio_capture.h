#pragma once
// Deterministic diagnostic mixer. Not a full-MAME filter/resampler equivalence claim.
#include <array>
#include <vector>
#include <cstdint>
#include <fstream>
#include <algorithm>
#include <stdexcept>
struct SunsetAudioCapture {
    using Stereo=std::array<int32_t,2>;
    std::vector<Stereo> fm{{0,0}},pcm{{0,0}};
    static void push(std::vector<Stereo>& v,int32_t l,int32_t r){
        if(v.size()>4000000)throw std::runtime_error("diagnostic audio capture limit");
        v.push_back({l,r});
    }
    static int64_t interpolate(const std::vector<Stereo>& v,uint64_t sample,unsigned ch){
        constexpr uint64_t denominator=48000ull*64;
        uint64_t position=sample*3579545,index=position/denominator,remainder=position%denominator;
        index=std::min<uint64_t>(index,v.size()-1);
        int64_t a=v[index][ch],b=v[std::min<uint64_t>(index+1,v.size()-1)][ch];
        return a+(b-a)*int64_t(remainder)/int64_t(denominator);
    }
    std::vector<int16_t> mix(uint64_t master_ticks)const{
        if(master_ticks>60ull*48000000)throw std::runtime_error("diagnostic duration exceeds 60 seconds");
        uint64_t samples=master_ticks/1000; // exact 48MHz -> 48kHz, floor.
        std::vector<int16_t> out;out.reserve(samples*2);
        for(uint64_t i=0;i<samples;++i)for(unsigned c=0;c<2;++c){
            int64_t v=(10*interpolate(fm,i,c)+7*interpolate(pcm,i,c))/10;
            out.push_back(int16_t(std::clamp<int64_t>(v,-32768,32767)));
        }
        return out;
    }
    void wav(const std::string& path,uint64_t ticks)const{
        auto data=mix(ticks);std::ofstream f(path,std::ios::binary);
        if(!f)throw std::runtime_error("cannot write private audio diagnostic");
        auto le=[&](uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)f.put(char((v>>(8*i))&255));};
        f.write("RIFF",4);le(36+data.size()*2,4);f.write("WAVEfmt ",8);le(16,4);le(1,2);le(2,2);
        le(48000,4);le(192000,4);le(4,2);le(16,2);f.write("data",4);le(data.size()*2,4);
        for(auto v:data)le(uint16_t(v),2);
        if(!f)throw std::runtime_error("audio write failed");
    }
};
inline void audio_capture_test(){
    SunsetAudioCapture c;c.fm.assign(56000,{1000,-1000});c.pcm.assign(56000,{1000,-1000});
    auto s=c.mix(48000000);if(s.size()!=96000||s[100]!=1700||s[101]!=-1700)throw std::runtime_error("audio duration/routing/gain fixture");
    c.fm.assign(56000,{50000,-50000});s=c.mix(1000);
    if(s.size()!=2||s[0]!=32767||s[1]!=-32768)throw std::runtime_error("audio clipping fixture");
    c.fm={{0,0},{1000,-1000},{2000,-2000}};c.pcm={{0,0},{0,0},{0,0}};s=c.mix(2000);
    if(s[0]!=0||s[1]!=0||s[2]!=1165||s[3]!=-1165)throw std::runtime_error("rational interpolation fixture");
    bool bad=false;try{c.mix(301ull*48000000);}catch(const std::runtime_error&){bad=true;}
    if(!bad)throw std::runtime_error("unbounded capture accepted");
}
