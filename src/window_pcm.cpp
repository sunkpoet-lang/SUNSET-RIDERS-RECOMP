// Unchanged MAME K053260 runs inside a private namespace over a generated host shim.
#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>
#include "pcm_guard_adapter.h"
#include "pcm_range_guard.h"
namespace kp {
#include "emu.h"
#include "k053260.cpp"
class ExposedPcm:public k053260_device{
public:
    ExposedPcm(const machine_config& c,const char* tag):k053260_device(c,tag,nullptr,3579545){}
    void timer_tick(){update_state_outputs(0);}
};
}
struct StatePcm::Impl {
    std::vector<uint8_t> rom;
    kp::machine_config config;
    std::string tag;
    kp::ExposedPcm chip;
    uint64_t rom_identity=14695981039346656037ull;
    uint64_t clock=0,next_timer=16,next_sample=64,count=0,nonzero=0;
    uint64_t hash=14695981039346656037ull,edges=0;
    int32_t last_sample[2]={};
    std::function<void(int32_t,int32_t)> capture;
    std::function<void(bool,uint64_t)> sh1;
    static std::string unique_tag(){static unsigned n=0;return "sunset_pcm_"+std::to_string(++n);}
    Impl(std::vector<uint8_t> bytes):rom(std::move(bytes)),tag(unique_tag()),chip(config,tag.c_str()){
        if(rom.size()!=0x100000)throw std::runtime_error("expected1MiB sample region");
        for(auto byte:rom)rom_identity=(rom_identity^byte)*1099511628211ull;
        kp::set_region(tag,rom.data(),rom.size());
        chip.sh1_cb().set([this](int state){++edges;if(sh1)sh1(state!=0,(clock*48000000+3579544)/3579545);});
        chip.start();chip.reset();
    }
    ~Impl(){kp::region_table().erase(tag);}
    void advance(uint64_t ticks){
        uint64_t target=(ticks/48000000)*3579545+(ticks%48000000)*3579545/48000000;
        if(target<clock)throw std::runtime_error("PCM clock moved backwards");
        while(true){
            uint64_t event=std::min(next_timer,next_sample);
            if(event>target)break;
            clock=event;
            // Native diagnostic tie policy: output-line timer, then sample.
            if(next_timer==event){chip.timer_tick();next_timer+=16;}
            if(next_sample==event){
                chip.generate(1);bool audible=false;
                for(unsigned channel=0;channel<2;++channel){
                    last_sample[channel]=int32_t(std::lround(chip.stream().m_outputs[channel][0]*32768.0f));
                    audible|=last_sample[channel]!=0;
                    uint32_t value=uint32_t(last_sample[channel]);
                    for(unsigned b=0;b<4;++b){hash^=(value>>(b*8))&255;hash*=1099511628211ull;}
                }
                if(capture)capture(last_sample[0],last_sample[1]);
                ++count;if(audible)++nonzero;next_sample+=64;
            }
        }
        clock=target;
    }
};
StatePcm::StatePcm(std::vector<uint8_t> rom):impl(std::make_unique<Impl>(std::move(rom))){}
StatePcm::~StatePcm()=default;
void StatePcm::advance_master(uint64_t ticks){impl->advance(ticks);}
uint8_t StatePcm::read(unsigned reg){return impl->chip.read(reg);}
void StatePcm::write(unsigned reg,uint8_t value){impl->chip.write(reg,value);}
uint8_t StatePcm::main_read(unsigned reg){return impl->chip.main_read(reg);}
void StatePcm::main_write(unsigned reg,uint8_t value){impl->chip.main_write(reg,value);}
void StatePcm::on_sh1(std::function<void(bool,uint64_t)> cb){impl->sh1=std::move(cb);}
uint64_t StatePcm::samples()const{return impl->count;}
uint64_t StatePcm::nonzero_samples()const{return impl->nonzero;}
uint64_t StatePcm::digest()const{return impl->hash;}
uint64_t StatePcm::sh1_edges()const{return impl->edges;}
int32_t StatePcm::last(unsigned channel)const{return impl->last_sample[channel];}

void StatePcm::on_native_sample(std::function<void(int32_t,int32_t)> cb){impl->capture=std::move(cb);}

StatePcm::Snapshot StatePcm::snapshot()const{
    auto fields=impl->chip.state_registry.capture();auto checksum=PrimitiveRegistry::checksum(fields);
    return {std::move(fields),{impl->clock,impl->next_timer,impl->next_sample,impl->count,impl->nonzero,impl->hash,impl->edges},
        {impl->last_sample[0],impl->last_sample[1]},checksum,impl->rom_identity,1};
}
void StatePcm::validate_component(const Snapshot& s)const{
    if(s.schema!=1||s.rom_identity!=impl->rom_identity||PrimitiveRegistry::checksum(s.chip)!=s.checksum)throw std::runtime_error("invalid PCM state identity/checksum");
    auto [clock,timer,sample,count,nonzero,hash,edges]=s.host;
    if(timer<=clock||timer-clock>16||sample<=clock||sample-clock>64||nonzero>count)throw std::runtime_error("invalid PCM adapter timing");
    impl->chip.state_registry.validate(s.chip);
    pcm_guard::validate(s);
}
void StatePcm::restore_component(const Snapshot& s){
    validate_component(s);
    auto [clock,timer,sample,count,nonzero,hash,edges]=s.host;
    auto prepared=s.chip; // Finish allocations before changing any chip field.
    impl->chip.state_registry.restore(prepared);
    impl->clock=clock;impl->next_timer=timer;impl->next_sample=sample;impl->count=count;impl->nonzero=nonzero;impl->hash=hash;impl->edges=edges;
    impl->last_sample[0]=s.last[0];impl->last_sample[1]=s.last[1];
}
