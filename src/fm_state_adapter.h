#pragma once
// ymfm adapter in native chip-clock domain. Diagnostic output, not final game audio.
#include "ymfm_opm.h"
#include <algorithm>
#include <array>
#include <functional>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
#include <tuple>
#include <utility>
class SunsetFm : public ymfm::ymfm_interface {
public:
    static constexpr uint64_t never=std::numeric_limits<uint64_t>::max();
    uint64_t clock=0,busy_until=0,next_sample=64,samples=0,nonzero_samples=0;
    uint64_t digest=14695981039346656037ull,reads=0,writes=0,timer_firings=0;
    std::array<uint64_t,2> deadlines{never,never};
    bool irq=false;
    uint8_t selected=0;
    std::array<uint8_t,256> registers{};
    std::array<uint64_t,2> timer_counts{};
    struct RegisterEvent {uint64_t clock;uint8_t reg,value;bool operator==(const RegisterEvent&)const=default;};
    std::vector<RegisterEvent> timer_trace;
    std::function<void(int32_t,int32_t)> on_sample;
    SunsetFm():chip(*this){chip.reset();}
    static uint64_t chip_clock(uint64_t ticks){return (ticks/48000000)*3579545+(ticks%48000000)*3579545/48000000;}
    void advance_master(uint64_t ticks){advance_chip(chip_clock(ticks));}
    void advance_chip(uint64_t target){
        if(target<clock)throw std::runtime_error("YM2151 clock moved backwards");
        while(true){
            uint64_t event=std::min(next_sample,std::min(deadlines[0],deadlines[1]));
            if(event>target)break;
            clock=event;
            // Defined diagnostic tie policy: timer A, timer B, then native sample.
            for(unsigned timer=0;timer<2;++timer)if(deadlines[timer]==event){
                deadlines[timer]=never;++timer_firings;++timer_counts[timer];m_engine->engine_timer_expired(timer);
            }
            if(next_sample==event){
                ymfm::ym2151::output_data output;chip.generate(&output);++samples;
                if(output.data[0]||output.data[1])++nonzero_samples;
                for(unsigned channel=0;channel<2;++channel){
                    uint32_t value=uint32_t(output.data[channel]);
                    for(unsigned b=0;b<4;++b){digest^=(value>>(b*8))&255;digest*=1099511628211ull;}
                }
                if(on_sample)on_sample(output.data[0],output.data[1]);
                next_sample+=64;
            }
        }
        clock=target;
    }
    uint8_t read(unsigned offset){++reads;return chip.read(offset);}
    void write(unsigned offset,uint8_t value){
        ++writes;
        if(!(offset&1))selected=value;
        else{
            registers[selected]=value;
            if((selected==0x10||selected==0x11||selected==0x12||selected==0x14)&&timer_trace.size()<256)
                timer_trace.push_back({clock,selected,value});
        }
        chip.write(offset,value);
    }
    uint8_t diagnostic_status(){return chip.read(1);}
    void reg(uint8_t address,uint8_t value){write(0,address);write(1,value);}
    void ymfm_set_timer(uint32_t timer,int32_t duration)override{
        deadlines.at(timer)=duration<0?never:clock+uint64_t(duration);
    }
    void ymfm_set_busy_end(uint32_t duration)override{busy_until=clock+duration;}
    bool ymfm_is_busy()override{return clock<busy_until;}
    void ymfm_update_irq(bool asserted)override{irq=asserted;} // Not connected to Z80 IRQ on this board.

    using HostState=std::tuple<decltype(clock),decltype(busy_until),decltype(next_sample),decltype(samples),decltype(nonzero_samples),decltype(digest),decltype(reads),decltype(writes),decltype(timer_firings),decltype(deadlines),decltype(irq),decltype(selected),decltype(registers),decltype(timer_counts),decltype(timer_trace)>;
    struct Snapshot {std::vector<uint8_t> chip_bytes;HostState host;uint64_t checksum;unsigned schema=1;bool operator==(const Snapshot&)const=default;};
    static uint64_t state_checksum(const std::vector<uint8_t>& bytes){uint64_t h=14695981039346656037ull;for(auto b:bytes)h=(h^b)*1099511628211ull;return h;}
    Snapshot snapshot(){
        std::vector<uint8_t> bytes;ymfm::ymfm_saved_state s(bytes,true);chip.save_restore(s);
        auto checksum=state_checksum(bytes);return {std::move(bytes),HostState(clock,busy_until,next_sample,samples,nonzero_samples,digest,reads,writes,timer_firings,deadlines,irq,selected,registers,timer_counts,timer_trace),checksum,1};
    }
    void restore_component(const Snapshot& s){
        auto current=snapshot();
        uint64_t time=std::get<0>(s.host),next=std::get<2>(s.host);
        if(s.schema!=1||s.chip_bytes.size()!=current.chip_bytes.size()||state_checksum(s.chip_bytes)!=s.checksum)
            throw std::runtime_error("invalid FM component bytes/schema");
        if(next<=time||next-time>64||std::get<4>(s.host)>std::get<3>(s.host)||std::get<14>(s.host).size()>256)
            throw std::runtime_error("invalid FM adapter state");
        for(auto deadline:std::get<9>(s.host))if(deadline!=never&&deadline<time)throw std::runtime_error("past FM timer deadline");
        auto bytes=s.chip_bytes;auto prepared=s.host; // Complete allocations before mutation.
        ymfm::ymfm_saved_state restore(bytes,false);chip.save_restore(restore);
        std::tie(clock,busy_until,next_sample,samples,nonzero_samples,digest,reads,writes,timer_firings,deadlines,irq,selected,registers,timer_counts,timer_trace)=std::move(prepared);
    }
private:
    ymfm::ym2151 chip;
};
