#include "sunset_session_profile.h"
#pragma once
#include "pcm_guard_adapter.h"
#include <algorithm>
namespace pcm_guard {
template<class T>T field(const StatePcm::Snapshot&s,const std::string&name){
    auto it=std::find_if(s.chip.begin(),s.chip.end(),[&](const auto&f){return f.name==name;});
    if(it==s.chip.end()||it->bytes.size()!=PrimitiveRegistry::wire_size<T>())throw std::runtime_error("missing PCM range field");
    T value{};size_t pos=0;PrimitiveRegistry::decode(value,it->bytes,pos,true);return value;
}
// The registry handles C-array shape; use an owning std::array only at this edge.
inline std::array<int32_t,2> gains(const StatePcm::Snapshot&s,const std::string&name){
    auto it=std::find_if(s.chip.begin(),s.chip.end(),[&](const auto&f){return f.name==name;});
    if(it==s.chip.end()||it->bytes.size()!=8)throw std::runtime_error("missing PCM gain field");
    std::array<int32_t,2> out{};size_t pos=0;for(auto&v:out)PrimitiveRegistry::decode(v,it->bytes,pos,true);return out;
}
inline constexpr int pan[8][2]={{0,0},{127,0},{116,52},{104,73},{90,90},{73,104},{52,116},{0,127}};
inline void validate(const StatePcm::Snapshot&s){
    constexpr uint64_t max_clock=sunset_session::admission_seconds*3579545;
    auto clock=s.host[0],ticks=clock/16;
    if(clock>max_clock||s.host[1]!=(clock/16+1)*16||s.host[2]!=(clock/64+1)*64||s.host[3]!=clock/64||s.host[4]>s.host[3])throw std::runtime_error("invalid PCM clock/grid/sample count");
    auto edges=(ticks/4)*2+std::min<uint64_t>(ticks%4,2);
    if(s.host[6]!=edges||field<uint32_t>(s,"m_timer_state")!=ticks%4||field<uint32_t>(s,"m_tim2_count")!=(ticks/4+(ticks%4>=2))%112)throw std::runtime_error("invalid PCM timer phase/TIM2/SH1 count");
    for(auto value:s.last)if(value<-32768||value>32767)throw std::runtime_error("invalid PCM cached output");
    for(unsigned i=0;i<4;++i){auto suffix="/"+std::to_string(i);
        auto pitch=field<uint16_t>(s,"m_pitch"+suffix);auto start=field<uint32_t>(s,"m_start"+suffix);
        auto p=field<uint8_t>(s,"m_pan"+suffix),v=field<uint8_t>(s,"m_volume"+suffix);
        if(pitch>4095||start>0x1fffff||p>7||v>127||field<uint16_t>(s,"m_counter"+suffix)>0x103f||field<uint32_t>(s,"m_position"+suffix)>0x20000)throw std::runtime_error("invalid PCM voice range");
        auto gain=gains(s,"m_pan_volume"+suffix);
        if(gain[0]!=v*pan[p][0]||gain[1]!=v*pan[p][1])throw std::runtime_error("inconsistent PCM pan gain");
    }
}
}
