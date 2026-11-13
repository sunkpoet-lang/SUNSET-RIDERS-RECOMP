#pragma once
// CPU-visible K053244/K053245 subset. No sprite rasterizer.
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>
struct SunsetSprites {
    std::array<uint16_t,0x2000> shared{};
    std::array<uint16_t,0x400> ram{},buffer{};
    std::array<uint8_t,16> regs{};
    std::vector<uint8_t> rom;
    unsigned rom_bank=0;
    uint64_t ram_writes=0,register_writes=0,buffer_updates=0,buffer_clears=0,rom_reads=0;
    static uint16_t combine(uint16_t old,uint16_t data,uint16_t mask){return (old&~mask)|(data&mask);}
    static unsigned index(unsigned offset){return ((offset&0xe)>>1)|((offset&0x1fc0)>>3);}
    uint16_t read_ram(unsigned offset)const{
        return (offset&0x31)?shared.at(offset):ram.at(index(offset));
    }
    void write_ram(unsigned offset,uint16_t data,uint16_t mask){
        shared.at(offset)=combine(shared.at(offset),data,mask);
        if(!(offset&0x31))ram.at(index(offset))=combine(ram.at(index(offset)),data,mask);
        if(mask)++ram_writes;
    }
    void update(){buffer=ram;++buffer_updates;}
    void clear(){for(unsigned i=0;i<buffer.size();i+=8)buffer[i]=0;++buffer_clears;}
    void write_register(unsigned offset,uint8_t data){
        regs.at(offset)=data;++register_writes;
        if(offset==6)update();else if(offset==7)clear();
    }
    uint8_t read_register(unsigned offset,bool side_effects){
        if(offset==6){if(side_effects)update();return 0;}
        if(offset==7){if(side_effects)clear();return 0;}
        if(offset>=12&&offset<=15){
            if(rom.size()!=0x200000)throw std::runtime_error("sprite ROM unavailable");
            unsigned address=(rom_bank<<19)|((regs[11]&7)<<18)|(regs[8]<<10)|(regs[9]<<2)|((offset&3)^1);
            ++rom_reads;return rom[address&(rom.size()-1)];
        }
        return 0;
    }
};
