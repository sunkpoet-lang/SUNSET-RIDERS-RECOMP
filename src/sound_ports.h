#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
struct SunsetSoundPorts {
    std::array<uint8_t,4> ports{};
    bool synchronised=false,irq_held=false,nmi_line=false;
    uint64_t irq_requests=0,nmi_block_until=0;
    uint8_t main_read(unsigned offset)const{
        if(!synchronised)throw std::runtime_error("sound_cpu_not_synchronised");
        return ports[2+(offset&1)];
    }
    void main_write(unsigned offset,uint8_t value){ports[offset&1]=value;}
    uint8_t sound_read(unsigned offset)const{
        offset&=0x3f;
        if(offset<2)return ports[offset];
        throw std::runtime_error("pcm_voice_register_not_implemented");
    }
    void sound_write(unsigned offset,uint8_t value){
        offset&=0x3f;
        if(offset==2||offset==3){ports[offset]=value;return;}
        throw std::runtime_error("pcm_voice_register_not_implemented");
    }
    void trigger_irq(){irq_held=true;++irq_requests;}
    void acknowledge_irq(){irq_held=false;}
    void arm_nmi(uint64_t ticks){nmi_line=false;nmi_block_until=ticks+24;}
    void sh1(bool level,uint64_t ticks){if(level&&ticks>=nmi_block_until)nmi_line=true;}
};
