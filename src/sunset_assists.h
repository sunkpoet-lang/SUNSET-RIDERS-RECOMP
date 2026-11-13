#pragma once
#include "sunset_menu_policy.h"
#include <array>
#include <cstdint>
#include <stdexcept>
namespace sunset_menu {
// Independently derived EAC pickup mailbox semantics; no imported cheat interpreter.
class Assists {
public:
    struct Snapshot {std::array<bool,4> lives{};std::array<uint8_t,4> pending{},age{};};
    enum Result {None,Applied,AlreadyOwned,Inactive,Busy,Denied};
    static std::array<uint8_t,32> expected_digest(){
        constexpr char hex[]="857a1661ea4a25e4f28fb72f8d8592ecdd34307a96bd5097286d329ea392d504";
        std::array<uint8_t,32> d{};auto n=[](char c){return c<='9'?c-'0':c-'a'+10;};
        for(unsigned i=0;i<32;++i)d[i]=uint8_t(n(hex[2*i])*16+n(hex[2*i+1]));return d;
    }
private:
    Snapshot state_{};bool verified_=false;
public:
    std::array<Result,4> last{};std::array<uint64_t,4> topups_by_player{};uint64_t topups=0,pickups=0;
    bool bind(const std::array<uint8_t,32>&digest){verified_=digest==expected_digest();if(!verified_)state_={};return verified_;}
    const Snapshot& snapshot()const{return state_;}
    bool infinite(unsigned p)const{return p<4&&state_.lives[p];}
    bool set_lives(unsigned p,bool on,Policy&policy){if(p>=4||!verified_||!policy.allows(Action::Assist))return false;state_.lives[p]=on;return true;}
    bool request(unsigned p,unsigned mask,Policy&policy){if(p>=4||mask<1||mask>3||!verified_||!policy.allows(Action::Assist))return false;state_.pending[p]=uint8_t(mask);state_.age[p]=0;last[p]=None;return true;}
    void restore(const Snapshot&s){if(!verified_)throw std::runtime_error("unverified assist profile");for(unsigned p=0;p<4;++p)if(s.pending[p]>3||s.age[p]>60)throw std::runtime_error("invalid own-slot assist metadata");state_=s;last.fill(None);}
    template<class Board> void tick(Board&b,Policy&policy){
        if(!verified_||!policy.allows(Action::Assist)){state_={};last.fill(Denied);return;}
        auto rb=[&](unsigned a){auto w=b.read(a&~1u,0xffff,false);return uint8_t((a&1)?w:w>>8);};
        auto wb=[&](unsigned a,uint8_t v){b.write(a&~1u,(a&1)?v:uint16_t(v)<<8,(a&1)?0xff:0xff00);};
        for(unsigned p=0;p<4;++p){
            unsigned record=0x104200+p*0x40,base=0x181c80+p*0x300;
            bool active=b.ram.at(5)==4&&(b.ram.at(0x170)&(1u<<p))&&b.ram.at(0x200+p*0x40)>0;
            if(!active){if(state_.pending[p])last[p]=Inactive;state_.pending[p]=state_.age[p]=0;continue;}
            if(state_.lives[p]&&b.ram[0x200+p*0x40]<3){wb(record,3);policy.mark_applied(Action::Assist);++topups;++topups_by_player[p];}
            if(!state_.pending[p])continue;
            if(rb(base+0x20)!=p||rb(base+0x31)!=3){last[p]=Inactive;state_.pending[p]=state_.age[p]=0;continue;}
            unsigned flags=rb(base+0x27);state_.pending[p]&=uint8_t(~flags);
            if(!state_.pending[p]){last[p]=AlreadyOwned;state_.age[p]=0;continue;}
            if(rb(base+0x6f)||(rb(base+0x60)&1)){
                if(++state_.age[p]>=60){last[p]=Busy;state_.pending[p]=state_.age[p]=0;}continue;
            }
            uint8_t bit=(state_.pending[p]&1)?1:2;
            wb(base+0x6f,bit==1?14:15);wb(base+0x60,rb(base+0x60)|1);
            state_.pending[p]&=uint8_t(~bit);state_.age[p]=0;last[p]=Applied;
            policy.mark_applied(Action::Assist);++pickups;
        }
    }
};
}
