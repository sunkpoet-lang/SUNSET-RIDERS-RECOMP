#pragma once
// EAC ssridr4p active-low input ports, from MAME tmnt2.cpp/konamipt.h.
#include <array>
#include <cstdint>
#include <stdexcept>
struct SunsetInputs {
    // left,right,up,down,button1,button2. EAC four-player has no Start bits.
    std::array<uint8_t,4> held{};
    uint8_t coins=0,services=0;
    void frame(const std::array<uint8_t,4>& players,uint8_t c,uint8_t s){
        for(auto p:players)if(p&0xc0)throw std::runtime_error("invalid EAC player control bits");
        if((c|s)&0xf0)throw std::runtime_error("invalid EAC coin/service bits");
        held=players;coins=c;services=s;
    }
    uint8_t player(unsigned p)const{return uint8_t(~held.at(p));}
    uint8_t coin_port()const{return uint8_t(~(coins|(services<<4)));}
};
