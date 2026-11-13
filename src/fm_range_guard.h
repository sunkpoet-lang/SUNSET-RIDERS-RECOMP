#include "sunset_session_profile.h"
#pragma once
#include <tuple>
#include <cstdint>
#include <stdexcept>
namespace fm_guard {
inline constexpr size_t wire_bytes=648,operators_begin=327;
template<class Snapshot>void validate(const Snapshot&s){
    const auto&b=s.chip_bytes;const auto&h=s.host;
    if(b.size()!=wire_bytes)throw std::runtime_error("unsupported pinned YM2151 wire layout");
    uint64_t time=std::get<0>(h),samples=std::get<3>(h);constexpr uint64_t never=~uint64_t(0);
    if(time>sunset_session::admission_seconds*3579545||std::get<2>(h)!=(time/64+1)*64||samples!=time/64||std::get<4>(h)>samples)throw std::runtime_error("invalid YM2151 clock/grid/sample count");
    auto busy=std::get<1>(h);if(busy>time&&busy-time>64)throw std::runtime_error("invalid YM2151 busy end");
    if((b[4]&~3u)||b[5]!=2||b[6]!=3||b[7]>1||b[8]>1||b[9]>1||b[10]!=uint8_t(samples))throw std::runtime_error("invalid YM2151 engine flags/prescale/clocks");
    if(b[7]!=uint8_t((b[4]&3)!=0)||bool(b[7])!=std::get<10>(h)||b[20]>31||b[21]>1||b[647]!=std::get<11>(h))throw std::runtime_error("inconsistent YM2151 IRQ/noise/address");
    const auto&deadlines=std::get<9>(h);const auto&counts=std::get<13>(h);
    for(unsigned i=0;i<2;++i){auto d=deadlines[i];
        if(bool(b[8+i])!=(d!=never)||(d!=never&&(d<=time||d-time>(i?262144u:65536u)))||counts[i]>time/64+1)throw std::runtime_error("invalid YM2151 timer running/deadline/count");
    }
    if(std::get<8>(h)!=counts[0]+counts[1])throw std::runtime_error("inconsistent YM2151 timer firing count");
    const auto&trace=std::get<14>(h);if(trace.size()>256)throw std::runtime_error("YM2151 timer trace exceeds bound");uint64_t previous=0;
    for(const auto&e:trace){if(e.clock<previous||e.clock>time||(e.reg!=0x10&&e.reg!=0x11&&e.reg!=0x12&&e.reg!=0x14))throw std::runtime_error("invalid YM2151 timer trace");previous=e.clock;}
    for(unsigned op=0;op<32;++op){size_t p=operators_begin+10*op;unsigned attenuation=unsigned(b[p+4])|(unsigned(b[p+5])<<8);
        if(attenuation>1023||b[p+6]<1||b[p+6]>4||b[p+7]!=0||b[p+8]>1||(b[p+9]&~5u))throw std::runtime_error("invalid YM2151 envelope/SSG/key field");
    }
}
}
