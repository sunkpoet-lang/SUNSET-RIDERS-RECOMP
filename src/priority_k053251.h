#pragma once
// CPU-visible K053251 model; palette indices are not final RGB colours.
#include <array>
#include <cstdint>
struct SunsetPriority {
    std::array<uint8_t,16> registers{};
    uint64_t writes=0;
    void write(unsigned offset,uint8_t value){registers[offset&15]=value&0x3f;++writes;}
    uint8_t priority(unsigned ci)const{return registers[ci&15];}
    uint8_t palette(unsigned ci)const{
        if(ci<3)return uint8_t(32*((registers[9]>>(ci*2))&3));
        return uint8_t(16*((registers[10]>>((ci-3)*3))&7));
    }
};
