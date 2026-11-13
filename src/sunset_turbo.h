#pragma once
#include "sunset_menu_policy.h"
#include <array>
#include <cstdint>
#include <stdexcept>
namespace sunset_menu {
class Turbo {
public:
    static constexpr uint64_t clock=48000000,frame_ticks=811008;
    struct Snapshot {std::array<unsigned,4> rate{};std::array<uint64_t,4> phase{};std::array<bool,4> held{};};
private:
    Snapshot state_;
    static bool valid_rate(unsigned r){return r==0||r==5||r==10||r==15;}
public:
    const Snapshot& snapshot()const{return state_;}
    unsigned rate(unsigned p)const{return p<4?state_.rate[p]:0;}
    bool set(unsigned p,unsigned rate,Policy&policy){
        if(p>=4||!valid_rate(rate)||!policy.allows(Action::Turbo))return false;
        state_.rate[p]=rate;state_.phase[p]=0;state_.held[p]=false;return true;
    }
    void release(unsigned p){if(p<4){state_.phase[p]=0;state_.held[p]=false;}}
    void restore(const Snapshot&s){
        for(unsigned p=0;p<4;++p)if(!valid_rate(s.rate[p])||s.phase[p]>=clock||(!s.held[p]&&s.phase[p]!=0))throw std::runtime_error("invalid private turbo snapshot");
        state_=s;
    }
    uint8_t apply(unsigned p,uint8_t input,Policy&policy){
        if(p>=4)return input;
        if(!policy.allows(Action::Turbo)){state_={};return input;}
        unsigned rate=state_.rate[p];bool fire=(input&16)!=0;
        if(!rate||!fire){release(p);return input;}
        policy.mark_applied(Action::Turbo);
        if(!state_.held[p])state_.phase[p]=0;
        state_.held[p]=true;bool on=state_.phase[p]<clock/2;
        state_.phase[p]=(state_.phase[p]+rate*frame_ticks)%clock;
        return on?input:uint8_t(input&~16);
    }
};
}
