#pragma once
// ER5911 128x8 subset, based on pinned MAME eepromser/eeprom behaviour.
// Independent model; source provenance and limitations are in OpenSpec.
#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
struct SunsetEeprom {
    enum State { Reset, Start, Command, Reading, Writing, Completion } state=Reset;
    std::array<uint8_t,128> data{};
    bool cs=false,clk=false,di=false,locked=true;
    uint64_t now=0,cs_rise=0,busy_until=0,commands=0,writes=0;
    unsigned bits=0,accumulator=0,address=0;
    uint32_t shift=0;
    explicit SunsetEeprom(const std::vector<uint8_t>& initial){
        if(initial.size()!=data.size())throw std::runtime_error("wrong EEPROM size");
        std::copy(initial.begin(),initial.end(),data.begin());
    }
    bool ready()const{return now>=busy_until;}
    bool output()const{return state==Reading?(shift&0x80000000u)!=0:true;}
    void advance(uint64_t tick){
        if(tick<now)throw std::runtime_error("EEPROM time moved backwards");
        now=tick;
    }
    void decode(){
        ++commands;bits=0;address=accumulator&0x1ff;
        unsigned op=accumulator>>9;
        if(op==0){
            unsigned sub=address>>7;
            if(sub==0){locked=true;state=Reset;}
            else if(sub==3){locked=false;state=Reset;}
            else if(sub==2){
                if(locked){state=Reset;return;}
                data.fill(0xff);busy_until=now+384000;state=Completion;++writes;
            }else throw std::runtime_error("ER5911 unsupported command");
            return;
        }
        if(address>=128)throw std::runtime_error("ER5911 address out of range");
        shift=0;
        state=op==2?Reading:Writing; // Both 01 and 11 are WRITE on ER5911.
    }
    void rising(){
        if(state==Start){
            if(di&&ready()&&now>cs_rise){bits=accumulator=0;state=Command;}
        }else if(state==Command){
            accumulator=(accumulator<<1)|unsigned(di);
            if(++bits==11)decode();
        }else if(state==Reading){
            if(bits++==0)shift=uint32_t(data[address])<<24;
            else shift=(shift<<1)|1;
        }else if(state==Writing){
            shift=(shift<<1)|unsigned(di);
            if(++bits==8){
                if(locked)state=Reset;
                else{data[address]=uint8_t(shift);busy_until=now+84000;state=Completion;++writes;}
            }
        }
    }
    // Driver callback order: DI, CS, CLK. A CLK rise at the CS-rise timestamp cannot start a command.
    void lines(bool input,bool select,bool clock,uint64_t tick){
        advance(tick);di=input;
        if(cs!=select){cs=select;if(cs){cs_rise=now;state=Start;}else state=Reset;}
        if(clk!=clock){clk=clock;if(clk&&cs)rising();}
    }
};
