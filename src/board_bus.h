#pragma once
// Protection subset adapted from MAME tmnt2.cpp: BSD-3-Clause, copyright Nicola Salmoria.
// Sunset Riders bring-up bus. Pending devices fail closed; no fake success reads.
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include "eeprom_er5911.h"
#include "priority_k053251.h"
#include "sprite_k053245.h"
#include "sound_ports.h"
#include "inputs_four.h"

struct BusStop : std::runtime_error {
    uint32_t address;
    bool write;
    BusStop(uint32_t a, bool w, const char* reason) : std::runtime_error(reason), address(a), write(w) {}
};

struct SunsetWatchdog {
    // MAME's unconfigured watchdog: cold reset leaves timer unarmed until first feed.
    static constexpr uint64_t timeout_ticks=3ull*48000000;
    uint64_t now=0,deadline=0,feeds=0;
    bool armed=false,soft_reset_requested=false;
    void advance(uint64_t tick){
        if(tick<now)throw std::runtime_error("watchdog time moved backwards");
        now=tick;
        if(armed&&now>=deadline){armed=false;soft_reset_requested=true;}
    }
    void feed(){deadline=now+timeout_ticks;armed=true;++feeds;}
};

// CPU-visible K052109 state only. Raster drawing/scroll latching is not implemented.
struct SunsetTileControl {
    std::vector<uint8_t> ram=std::vector<uint8_t>(0x4000);
    uint8_t irq_control=0,addrmap=0,scrollctrl=0,romsubbank=0,tileflip=0;
    uint8_t banks[4]={},banks2[4]={};
    bool irq=false,rmrd=false;
    uint64_t writes=0;
    uint8_t read(size_t offset,uint32_t address)const{
        if(rmrd)throw BusStop(address,false,"tile_rom_read_not_implemented");
        return ram.at(offset);
    }
    void write(size_t offset,uint8_t data){
        ram.at(offset)=data;++writes;
        switch(offset){
        case 0x1c00:addrmap=data;break;
        case 0x1c80:scrollctrl=data;break;
        case 0x1d00:
            if((irq_control&~data)&4)irq=false;
            irq_control=data;break;
        case 0x1d80:banks[0]=data&15;banks[1]=data>>4;break;
        case 0x1f00:banks[2]=data&15;banks[3]=data>>4;break;
        case 0x1e00:case 0x3e00:romsubbank=data;break;
        case 0x1e80:tileflip=data&7;break;
        case 0x3d80:banks2[0]=data&15;banks2[1]=data>>4;break;
        case 0x3f00:banks2[2]=data&15;banks2[3]=data>>4;break;
        }
    }
    void vblank(bool state){if(state&&(irq_control&4))irq=true;}
};

class SunsetBus {
public:
    std::vector<uint8_t> rom;
    std::vector<uint8_t> ram = std::vector<uint8_t>(0x4000);
    std::vector<uint8_t> palette = std::vector<uint8_t>(0x1000);
    std::vector<uint8_t> auxiliary = std::vector<uint8_t>(0x80);
    uint64_t reads=0, writes=0;
    std::map<uint32_t,uint64_t> unmapped_writes;
    SunsetWatchdog watchdog;
    std::function<void(bool)> video_edge_callback;
    SunsetTileControl tiles;
    SunsetEeprom eeprom;
    SunsetPriority priority;
    SunsetSprites sprites;
    SunsetInputs inputs;
    SunsetSoundPorts sound;
    // Installed together only by a connected CPU diagnostic; independent probes keep guards.
    std::function<uint8_t(unsigned)> connected_sound_read;
    std::function<void(unsigned,uint8_t)> connected_sound_write;
    std::function<void()> connected_sound_irq;
    uint64_t protection_reads=0,protection_writes=0;
    uint8_t dim_control=0,sprite_rom_bank=0;
    uint64_t eeprom_control_writes=0;
    uint8_t dim_video=0;
    bool coin_lines[2]={};
    uint64_t coin_counts[2]={},board_control_writes=0;
    static constexpr uint64_t frame_ticks=811008,visible_start=49152,vblank_start=737280;
    uint64_t master_ticks=0,vblank_entries=0,vblank_exits=0,status_reads=0;
    bool service_test=false;
    explicit SunsetBus(std::vector<uint8_t> bytes,std::vector<uint8_t> initial_eeprom=std::vector<uint8_t>(128,0xff))
        : rom(std::move(bytes)),eeprom(initial_eeprom) {
        if(rom.size()!=0xc0000) throw std::runtime_error("wrong program size");
    }
    bool vblank()const{
        auto phase=master_ticks%frame_ticks;
        return phase<visible_start||phase>=vblank_start;
    }
    void advance(uint64_t tick){
        if(tick<master_ticks)throw std::runtime_error("board time moved backwards");
        uint64_t position=master_ticks;
        while(position<tick){
            uint64_t phase=position%frame_ticks,base=position-phase,next;
            bool entering;
            if(phase<visible_start){next=base+visible_start;entering=false;}
            else if(phase<vblank_start){next=base+vblank_start;entering=true;}
            else{next=base+frame_ticks+visible_start;entering=false;}
            if(next>tick)break;
            if(video_edge_callback)video_edge_callback(entering);
            tiles.vblank(entering);
            if(entering)++vblank_entries;else ++vblank_exits;
            position=next;
        }
        master_ticks=tick;eeprom.advance(tick);watchdog.advance(tick);
        if(watchdog.soft_reset_requested)throw BusStop(0x1c0400,false,"watchdog_soft_reset_pending");
    }
    uint16_t status()const{
        return uint16_t(0x70|(service_test?0:0x80)|(vblank()?0:8)|(eeprom.ready()?2:0)|(eeprom.output()?1:0));
    }
    uint16_t read(uint32_t address, uint16_t mask=0xffff, bool side_effects=true) {
        address &= 0xffffff;
        if(address&1) throw BusStop(address,false,"unaligned_word");
        ++reads;
        if(address<0xc0000) return word(rom,address,mask);
        if(address>=0x104000 && address<0x108000) return word(ram,address-0x104000,mask);
        if(address>=0x140000 && address<0x141000) return word(palette,address-0x140000,mask);
        if(address>=0x1c0500 && address<0x1c0580) return word(auxiliary,address-0x1c0500,mask);
        if(address==0x1c0400){if(side_effects)watchdog.feed();return 0;}
        if(address>=0x1c0000&&address<=0x1c0006)return inputs.player((address-0x1c0000)/2)&mask;
        if(address==0x1c0100)return inputs.coin_port()&mask;
        if(address==0x1c0800){
            ++protection_reads;
            // Adapted from pinned MAME tmnt2.cpp ssriders_protection_r (BSD-3-Clause).
            const int data=word(ram,0x1a0a,0xffff),cmd=word(ram,0x18fc,0xffff);
            uint16_t result;
            switch(cmd){
            case 0x100b:result=0x64;break;
            case 0x6003:result=data&15;break;
            case 0x6004:result=data&31;break;
            case 0x6000:result=data&1;break;
            case 0:case 0x6007:result=data&255;break;
            case 0x8abc:{int y=-int(word(ram,0x1818,0xffff));
                int result_y=((y/8-4)&31)*64;
                int x=(int(word(ram,0x1cb0,0xffff))+int(word(ram,0xc8,0xffff))-6)/8+12;
                result=uint16_t(result_y+(x&63));break;}
            default:throw BusStop(address,false,"unknown_protection_command");
            }
            return result&mask;
        }
        if(address==0x1c0102){++status_reads;return status()&mask;}
        if(address>=0x5c0600&&address<=0x5c0603){
            if(!(mask&0xff))return 0;
            if(connected_sound_read)return connected_sound_read((address-0x5c0600)/2);
            if(!sound.synchronised)throw BusStop(address,false,"sound_cpu_not_synchronised");
            return sound.main_read((address-0x5c0600)/2);
        }
        if(address>=0x180000&&address<=0x183fff)return sprites.read_ram((address-0x180000)/2);
        if(address>=0x5a0000&&address<=0x5a001f){
            unsigned offset=((address-0x5a0000)/2)&~1u;
            // Both reads occur in the driver even for a byte access. Keep ambiguous
            // dual-trigger read order fail-closed until differential validation.
            if(offset==6&&side_effects)throw BusStop(address,false,"sprite_pair_read_order_unverified");
            if(offset>=12&&sprites.rom.empty())throw BusStop(address,false,"sprite_rom_unavailable");
            uint16_t low=sprites.read_register(offset+1,side_effects);
            uint16_t high=sprites.read_register(offset,side_effects);
            return low|(high<<8);
        }
        if(address>=0x600000&&address<=0x603fff){
            if(!mask)return 0;
            const size_t offset=(address-0x600000)/2;
            if(mask&0xff00)return uint16_t(tiles.read(offset,address))<<8;
            return tiles.read(offset+0x2000,address);
        }
        throw BusStop(address,false,pending(address)?"device_not_implemented":"unmapped_access");
    }
    void write(uint32_t address,uint16_t value,uint16_t mask=0xffff) {
        address &= 0xffffff;
        if(address&1) throw BusStop(address,true,"unaligned_word");
        ++writes;
        if(address<0xc0000) return; // MAME ROM mapping ignores data writes.
        if(address>=0x104000 && address<0x108000) return store(ram,address-0x104000,value,mask);
        if(address>=0x140000 && address<0x141000) return store(palette,address-0x140000,value,mask);
        if(address>=0x1c0500 && address<0x1c0580) return store(auxiliary,address-0x1c0500,value,mask);
        if(address==0x1c0400){watchdog.feed();return;}
        if(address==0x1c0800||address==0x1c0802){
            ++protection_writes;
            // MAME offset 0 has no effect. Offset 1 drives priority attributes.
            if(address==0x1c0802&&mask){
                unsigned hardware=1;
                for(unsigned logical=1;logical<256;logical<<=1)
                    for(unsigned i=0;i<128;++i)
                        if((sprites.read_ram(3+64*i)>>8)==logical){
                            auto& target=sprites.ram.at(8*i);
                            target=SunsetSprites::combine(target,uint16_t(hardware++),0xff);
                        }
            }
            return;
        }
        if(address==0x1c0200){
            if(mask&0xff){
                ++eeprom_control_writes;
                eeprom.lines(value&1,value&2,value&4,eeprom.now);
                dim_control=value&0x18;sprite_rom_bank=((value>>5)&1)<<2;
                sprites.rom_bank=sprite_rom_bank;
            }
            return;
        }
        if(address==0x1c0300){
            if(mask&0xff){
                ++board_control_writes;
                for(unsigned i=0;i<2;++i){
                    bool level=(value>>i)&1;
                    if(eeprom.now>0&&level&&!coin_lines[i])++coin_counts[i];
                    coin_lines[i]=level;
                }
                tiles.rmrd=(value&8)!=0;dim_video=(value>>4)&7;
            }
            return;
        }
        if(address>=0x5c0600&&address<=0x5c0603){
            if(mask&0xff){if(connected_sound_write)connected_sound_write((address-0x5c0600)/2,uint8_t(value));
                else sound.main_write((address-0x5c0600)/2,uint8_t(value));}
            return;
        }
        if(address==0x5c0604){
            if(connected_sound_irq){connected_sound_irq();return;}
            if(!sound.synchronised)throw BusStop(address,true,"sound_irq_delivery_pending");
            sound.trigger_irq();return;
        }
        if(address>=0x5c0700&&address<=0x5c071f){
            if(mask&0xff)priority.write((address-0x5c0700)/2,uint8_t(value));
            return;
        }
        if(address>=0x180000&&address<=0x183fff){sprites.write_ram((address-0x180000)/2,value,mask);return;}
        if(address>=0x5a0000&&address<=0x5a001f){
            unsigned offset=((address-0x5a0000)/2)&~1u;
            if(mask&0xff00)sprites.write_register(offset,uint8_t(value>>8));
            if(mask&0xff)sprites.write_register(offset+1,uint8_t(value));
            return;
        }
        if(address>=0x600000&&address<=0x603fff){
            if(!mask)return;
            const size_t offset=(address-0x600000)/2;
            if(mask&0xff00)tiles.write(offset,uint8_t(value>>8));
            else tiles.write(offset+0x2000,uint8_t(value));
            return;
        }
        if(pending(address)) throw BusStop(address,true,"device_not_implemented");
        // Pinned MAME emumem_heun.cpp: unmapped writes log, without mutation.
        ++unmapped_writes[address];
    }
private:
    static uint16_t word(const std::vector<uint8_t>& bytes,size_t i,uint16_t mask) {
        return uint16_t((uint16_t(bytes.at(i))<<8)|bytes.at(i+1)) & mask;
    }
    static void store(std::vector<uint8_t>& bytes,size_t i,uint16_t value,uint16_t mask) {
        uint16_t old=uint16_t((uint16_t(bytes.at(i))<<8)|bytes.at(i+1));
        uint16_t next=uint16_t((old&~mask)|(value&mask));
        bytes.at(i)=uint8_t(next>>8);bytes.at(i+1)=uint8_t(next);
    }
    static bool pending(uint32_t a) {
        return (a>=0x180000&&a<=0x183fff)||(a>=0x1c0000&&a<=0x1c0007)
         ||(a>=0x1c0100&&a<=0x1c0103)||(a>=0x1c0200&&a<=0x1c0201)
         ||(a>=0x1c0300&&a<=0x1c0301)||(a>=0x1c0400&&a<=0x1c0401)
         ||(a>=0x1c0800&&a<=0x1c0803)||(a>=0x5a0000&&a<=0x5a001f)
         ||(a>=0x5c0600&&a<=0x5c0605)||(a>=0x5c0700&&a<=0x5c071f)
         ||(a>=0x600000&&a<=0x603fff);
    }
};
