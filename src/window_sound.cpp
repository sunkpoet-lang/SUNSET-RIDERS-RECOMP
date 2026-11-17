#include "sunset_session_profile.h"
#include "fm_range_tests.h"
#include "pcm_range_tests.h"
#include "cpu_boundary_guard.h"
#include "state_tuple.h"
// Isolated Z80 interpreter diagnostic. No PCM/FM voices or audio output.
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
#include <iostream>
#include "sound_ports.h"
#include "fm_guard_adapter.h"
#include "pcm_guard_adapter.h"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace sz {
#include "emu.h"
#include "logmacro.h"
#include "z80.h"
}
struct SoundStop:std::runtime_error{
    uint16_t address;bool write;
    SoundStop(uint16_t a,bool w,const char* reason):std::runtime_error(reason),address(a),write(w){}
};
struct SoundBoard{
    std::vector<uint8_t> rom=std::vector<uint8_t>(0x10000);
    std::array<uint8_t,0x800> ram{};
    SunsetSoundPorts ports;
    SunsetFm fm;
    StatePcm pcm;
    std::function<void(bool)> nmi_changed;
    uint64_t ticks=0,reads=0,writes=0,unmapped_reads=0,unmapped_writes=0;
    explicit SoundBoard(std::vector<uint8_t> samples=std::vector<uint8_t>(0x100000,0xff)):pcm(std::move(samples)){
        pcm.on_sh1([this](bool level,uint64_t time){
            bool before=ports.nmi_line;ports.sh1(level,time);
            if(before!=ports.nmi_line&&nmi_changed)nmi_changed(ports.nmi_line);
        });
    }
    void advance(uint64_t time){ticks=time;fm.advance_master(time);pcm.advance_master(time);}
    void main_write(unsigned p,uint8_t value){pcm.main_write(p,value);}
    uint8_t main_read(unsigned p){
        if(!ports.synchronised)throw std::runtime_error("sound_cpu_not_synchronised");
        return pcm.main_read(p);
    }
    uint8_t read(uint16_t a){
        ++reads;
        if(a<0xf000)return rom[a];
        if(a<0xf800)return ram[a-0xf000];
        if(a==0xf800||a==0xf801){fm.advance_master(ticks);return fm.read(a-0xf800);}
        if(a>=0xfa00&&a<=0xfa2f){
            pcm.advance_master(ticks);return pcm.read(a-0xfa00);
        }
        ++unmapped_reads;return 0; // Pinned address-map default and MAME unmapped handler.
    }
    void write(uint16_t a,uint8_t v){
        ++writes;
        if(a<0xf000)return;
        if(a<0xf800){ram[a-0xf000]=v;return;}
        if(a==0xf800||a==0xf801){fm.advance_master(ticks);fm.write(a-0xf800,v);return;}
        if(a>=0xfa00&&a<=0xfa2f){
            pcm.advance_master(ticks);pcm.write(a-0xfa00,v);return;
        }
        if(a==0xfc00){ports.arm_nmi(ticks);if(nmi_changed)nmi_changed(false);return;}
        ++unmapped_writes; // MAME unmapped writes log without storage.
    }
};
class SoundCpu:public sz::z80_device{
public:SoundCpu(const sz::machine_config& c):sz::z80_device(c,"soundcpu",nullptr,8000000){
        m_tmp_irq_vector=0;m_shared_data.w=0;m_shared_data2.w=0;m_icount=0;
    }
    auto snapshot()const{return std::make_pair(state_host_total_cycles(),std::make_tuple(state_own(m_prvpc.w),state_own(m_pc.w),state_own(m_sp.w),state_own(m_af.w),state_own(m_bc.w),state_own(m_de.w),state_own(m_hl.w),state_own(m_ix.w),state_own(m_iy.w),state_own(m_wz.w),state_own(m_af2.w),state_own(m_bc2.w),state_own(m_de2.w),state_own(m_hl2.w),state_own(m_r),state_own(m_r2),state_own(m_iff1),state_own(m_iff2),state_own(m_halt),state_own(m_im),state_own(m_i),state_own(m_nmi_state),state_own(m_irq_state),state_own(m_wait_state),state_own(m_busreq_state),state_own(m_busack_state),state_own(m_ea),state_own(m_service_attention),state_own(m_tmp_irq_vector),state_own(m_shared_data.w),state_own(m_shared_data2.w),state_own(m_irtmp.w),state_own(m_ref),state_own(m_icount),state_own(m_m1_cycles),state_own(m_mreq_cycles),state_own(m_iorq_cycles),state_own(m_f.s_val),state_own(m_f.z_val),state_own(m_f.yx_val),state_own(m_f.h_val),state_own(m_f.pv_val),state_own(m_f.n),state_own(m_f.c),state_own(m_f.q),state_own(m_f.qtemp)));}
    void restore_component(const auto& s){cpu_boundary::validate_sound(s);restore_state_tuple(s.second,std::tie(m_prvpc.w,m_pc.w,m_sp.w,m_af.w,m_bc.w,m_de.w,m_hl.w,m_ix.w,m_iy.w,m_wz.w,m_af2.w,m_bc2.w,m_de2.w,m_hl2.w,m_r,m_r2,m_iff1,m_iff2,m_halt,m_im,m_i,m_nmi_state,m_irq_state,m_wait_state,m_busreq_state,m_busack_state,m_ea,m_service_attention,m_tmp_irq_vector,m_shared_data.w,m_shared_data2.w,m_irtmp.w,m_ref,m_icount,m_m1_cycles,m_mreq_cycles,m_iorq_cycles,m_f.s_val,m_f.z_val,m_f.yx_val,m_f.h_val,m_f.pv_val,m_f.n,m_f.c,m_f.q,m_f.qtemp));state_host_restore_total_cycles(s.first);}

    uint16_t pc()const{return m_pc.w;}
};
static void attach(SoundCpu& cpu,SoundBoard& b){
    cpu.space(sz::AS_PROGRAM).read=[&](sz::offs_t a){return b.read(uint16_t(a));};
    cpu.space(sz::AS_PROGRAM).write=[&](sz::offs_t a,uint8_t v){b.write(uint16_t(a),v);};
    cpu.space(sz::AS_IO).read=[](sz::offs_t a)->uint8_t{throw SoundStop(uint16_t(a),false,"sound_io_unmapped");};
    cpu.space(sz::AS_IO).write=[](sz::offs_t a,uint8_t){throw SoundStop(uint16_t(a),true,"sound_io_unmapped");};
    b.nmi_changed=[&cpu](bool level){cpu.set_input_line(sz::INPUT_LINE_NMI,level?sz::ASSERT_LINE:sz::CLEAR_LINE);};
    cpu.start();cpu.reset();
}
static unsigned checks=0;
static void require(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
static void self_test(){
    SoundBoard b;
    require(b.ports.ports==std::array<uint8_t,4>{},"port cold zeros");
    bool stopped=false;try{b.main_read(0);}catch(const std::runtime_error&){stopped=true;}
    require(stopped,"main read cannot fake a disconnected response");
    b.main_write(0,0x5a);b.main_write(1,0xa5);
    require(b.read(0xfa00)==0x5a&&b.read(0xfa01)==0xa5,"main-to-sound ports");
    b.write(0xfa02,0x12);b.write(0xfa03,0x34);b.ports.synchronised=true;
    require(b.main_read(0)==0x12&&b.main_read(1)==0x34,"sound-to-main ports");
    b.main_write(2,0x77);require(b.read(0xfa00)==0x77,"main port mirror");
    b.ports.trigger_irq();require(b.ports.irq_held&&b.ports.irq_requests==1,"held IRQ request");
    b.ports.acknowledge_irq();require(!b.ports.irq_held,"IRQ clears on acknowledge");
    b.ports.arm_nmi(100);b.ports.sh1(true,123);require(!b.ports.nmi_line,"NMI blocked for four Z80 cycles");
    b.ports.sh1(true,124);require(b.ports.nmi_line,"NMI unblocks at deadline");
    b.ports.sh1(false,125);require(b.ports.nmi_line,"SH1 falling does not clear latched NMI");
    b.ports.arm_nmi(126);require(!b.ports.nmi_line,"rearm clears NMI");
    b.write(0xf000,0xab);b.write(0xf7ff,0xcd);
    require(b.read(0xf000)==0xab&&b.read(0xf7ff)==0xcd,"Z80 RAM boundaries");
    b.rom[0xefff]=0x91;b.write(0xefff,0);
    require(b.read(0xefff)==0x91,"Z80 ROM immutable and boundary correct");
    b.write(0xfffe,0x12);
    require(b.read(0xfffe)==0&&b.unmapped_writes==1&&b.unmapped_reads==1,"sound unmapped memory matches MAME default without storage");
    b.write(0xf800,0x20);b.write(0xf801,0xc7);
    require(b.read(0xf801)&0x80,"FM mapped data write generates actual busy flag");
    require(b.read(0xfa29)==0,"actual PCM cold voice status");
    // Synthetic Z80 code: LD A,42; LD(f000),A; LD A,(fa00); LD(fa02),A; HALT.
    const uint8_t program[]={0x3e,0x42,0x32,0x00,0xf0,0x3a,0x00,0xfa,0x32,0x02,0xfa,0x76};
    std::copy(std::begin(program),std::end(program),b.rom.begin());
    b.main_write(0,0x6d);sz::machine_config config;SoundCpu cpu(config);attach(cpu,b);cpu.run(1000);
    require(b.ram[0]==0x42,"actual Z80 core executes RAM store");
    require(b.main_read(0)==0x6d,"actual Z80 core transfers command response");
    SunsetFm fm;
    require(fm.read(1)==0,"YM2151 reset status");
    fm.write(0,0x20);require((fm.read(1)&0x80)==0,"address write alone does not set busy");
    fm.write(1,0xc7);require(fm.read(1)&0x80,"data write sets busy");
    fm.advance_chip(63);require(fm.read(1)&0x80,"busy one clock before expiry");
    fm.advance_chip(64);require((fm.read(1)&0x80)==0,"busy clears after64 chip clocks");
    fm.reg(0x10,0xff);fm.reg(0x11,3);fm.reg(0x14,5);
    auto timer=fm.deadlines[0];require(timer==128,"timer A shortest period is64 chip clocks");
    fm.advance_chip(timer-1);require(!(fm.read(1)&1),"timer A not early");
    fm.advance_chip(timer);require((fm.read(1)&1)&&fm.irq,"timer A flag and internal IRQ at deadline");
    fm.reg(0x14,0x10);require(!(fm.read(1)&1)&&!fm.irq,"timer A reset/disable acknowledges status");
    fm.reg(0x12,0xff);fm.reg(0x14,0x0a);
    timer=fm.deadlines[1];require(timer==fm.clock+(16-(fm.samples&15))*64,"timer B accounts for free-running divider phase");
    fm.advance_chip(timer);require(fm.read(1)&2,"timer B expiry flag");
    require(SunsetFm::chip_clock(48000000)==3579545,"independent YM crystal rational clock");
    SunsetFm timer_b_cold;timer_b_cold.reg(0x12,0xff);timer_b_cold.reg(0x14,0x0a);
    require(timer_b_cold.deadlines[1]==1024,"timer B full1024 clocks from zero divider phase");
    SunsetFm large,small;
    auto tone=[](SunsetFm& chip){
        chip.reg(0x20,0xc7);chip.reg(0x28,0x4a);chip.reg(0x30,0);
        for(unsigned offset:{0u,8u,16u,24u}){
            chip.reg(0x40+offset,1);chip.reg(0x60+offset,0);
            chip.reg(0x80+offset,0x1f);chip.reg(0xa0+offset,0);
            chip.reg(0xc0+offset,0);chip.reg(0xe0+offset,0x0f);
        }
        chip.reg(8,0x78);
    };
    tone(large);tone(small);large.advance_master(4800000);
    for(uint64_t tick=4800;tick<=4800000;tick+=4800)small.advance_master(tick);
    require(large.nonzero_samples>0,"synthetic tone produces native FM output");
    require(large.samples==small.samples&&large.digest==small.digest,"native sample digest invariant to advance chunking");
    std::vector<uint8_t> sample_rom(0x100000);sample_rom[0]=0x20;sample_rom[1]=0x40;sample_rom[2]=0xc0;sample_rom[3]=0x20;
    auto setup=[](StatePcm& chip,unsigned length){
        chip.write(8,0xc0);chip.write(9,0x0f);chip.write(10,length&255);chip.write(11,length>>8);
        chip.write(12,0);chip.write(13,0);chip.write(14,0);chip.write(15,127);
        chip.write(0x2c,4);chip.write(0x2f,2);
    };
    auto at=[](uint64_t clocks){return (clocks*48000000+3579544)/3579545;};
    StatePcm pcm(sample_rom);setup(pcm,2);
    require(pcm.read(0x29)==0,"PCM voices cold stopped");
    pcm.write(0x28,1);require(pcm.read(0x29)==1,"PCM key on sets actual voice status");
    pcm.advance_master(at(64));require(pcm.last(0)==11430&&pcm.last(1)==11430,"PCM first byte is start+1 with centre pan");
    pcm.write(0x28,1);pcm.advance_master(at(128));
    require(pcm.last(0)==-11430,"held key on does not restart and PCM is signed");
    pcm.advance_master(at(192));require(pcm.read(0x29)==0,"PCM sample ends after declared length");
    StatePcm loop(sample_rom);setup(loop,1);loop.write(0x2a,1);loop.write(0x28,1);
    loop.advance_master(at(128));require(loop.read(0x29)==1&&loop.last(0)==5715,"PCM loop wraps to start byte");
    loop.write(0x28,0);require(loop.read(0x29)==0,"PCM key off stops channel");
    auto adpcm_rom=sample_rom;adpcm_rom[1]=0x21;
    StatePcm adpcm(adpcm_rom);setup(adpcm,2);adpcm.write(0x2a,0x10);adpcm.write(0x28,1);
    adpcm.advance_master(at(64));require(adpcm.last(0)==178,"KADPCM low nibble decoded first");
    adpcm.advance_master(at(128));require(adpcm.last(0)==535,"KADPCM high nibble accumulated next");
    StatePcm reverse(sample_rom);setup(reverse,2);reverse.write(12,4);reverse.write(0x28,0x11);
    reverse.advance_master(at(64));require(reverse.last(0)==5715,"reverse PCM uses start-minus-position");
    StatePcm rom_test(sample_rom);rom_test.write(0x2f,1);
    require(rom_test.read(0x2e)==0x20&&rom_test.read(0x2e)==0x40,"actual sample ROM-test pointer increments");
    StatePcm pcm_large(sample_rom),pcm_small(sample_rom);
    for(auto chip:{&pcm_large,&pcm_small}){setup(*chip,2);chip->write(0x2a,1);chip->write(0x28,1);}
    pcm_large.advance_master(480000);
    for(uint64_t tick=480;tick<=480000;tick+=480)pcm_small.advance_master(tick);
    require(pcm_large.samples()==pcm_small.samples()&&pcm_large.digest()==pcm_small.digest(),"PCM chunk-invariant sample digest");
    require(pcm_large.sh1_edges()>0,"actual PCM output-line callbacks clocked");
    std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";
}
int independent_sound_main(int argc,char** argv){
    try{
        if(argc==2&&std::string(argv[1])=="--self-test"){self_test();return 0;}
        int budget=8000000;
        if(argc==3&&std::string(argv[1])=="--cycles"){
            size_t end=0;budget=std::stoi(argv[2],&end);
            if(end!=std::string(argv[2]).size()||budget<1||budget>16000000)throw std::runtime_error("invalid Z80 cycle budget");
        }else if(argc!=1)throw std::runtime_error("usage: z80_probe [--cycles 1..16000000] < private_ROM");
#ifdef _WIN32
        _setmode(_fileno(stdin),_O_BINARY);
#endif
        std::vector<uint8_t> program(0x10000),samples(0x100000);
        std::cin.read(reinterpret_cast<char*>(program.data()),program.size());
        if(std::cin.gcount()!=0x10000)throw std::runtime_error("expected verified Z80 ROM");
        std::cin.read(reinterpret_cast<char*>(samples.data()),samples.size());
        if(std::cin.gcount()!=0x100000||std::cin.peek()!=EOF)throw std::runtime_error("expected verified sample ROM after program");
        SoundBoard b(std::move(samples));b.rom=std::move(program);
        sz::machine_config config;SoundCpu cpu(config);uint64_t instructions=0;std::set<uint16_t> pcs;
        bool checksum_success_path=false;
        cpu.instruction_hook=[&](sz::offs_t a){++instructions;pcs.insert(uint16_t(a));if(a==0xa1)checksum_success_path=true;int elapsed=budget-cpu.cycles_left();if(elapsed>=0)b.advance(uint64_t(elapsed)*6);};
        try{attach(cpu,b);cpu.run(budget);std::cout<<"{\"status\":\"diagnostic_budget_exhausted\"";}
        catch(const SoundStop& s){std::cout<<"{\"status\":\"diagnostic_incomplete\",\"reason\":\""<<s.what()<<"\",\"address\":"<<s.address<<",\"write\":"<<(s.write?"true":"false");}
        std::cout<<",\"pc\":"<<cpu.pc()<<",\"instructions\":"<<instructions<<",\"unique_pcs\":"<<pcs.size()
         <<",\"cycle_budget\":"<<budget<<",\"eac_checksum_success_path_seen\":"<<(checksum_success_path?"true":"false")
         <<",\"reply_ports\":["<<unsigned(b.pcm.main_read(0))<<','<<unsigned(b.pcm.main_read(1))<<']'
         <<",\"ym2151_reads\":"<<b.fm.reads<<",\"ym2151_writes\":"<<b.fm.writes
         <<",\"fm_native_samples\":"<<b.fm.samples<<",\"fm_nonzero_samples\":"<<b.fm.nonzero_samples
         <<",\"fm_diagnostic_digest\":"<<b.fm.digest
         <<",\"pcm_native_samples\":"<<b.pcm.samples()<<",\"pcm_nonzero_samples\":"<<b.pcm.nonzero_samples()
         <<",\"pcm_diagnostic_digest\":"<<b.pcm.digest()<<",\"sh1_edges\":"<<b.pcm.sh1_edges()
         <<",\"unmapped_sound_reads\":"<<b.unmapped_reads<<",\"unmapped_sound_writes\":"<<b.unmapped_writes
         <<",\"ym_status\":"<<unsigned(b.fm.diagnostic_status())<<",\"ym_mode\":"<<unsigned(b.fm.registers[0x14])
         <<",\"ym_clock\":"<<b.fm.clock<<",\"ym_timer_counts\":["<<b.fm.timer_counts[0]<<','<<b.fm.timer_counts[1]
         <<"],\"ym_timer_deadlines\":["<<b.fm.deadlines[0]<<','<<b.fm.deadlines[1]<<"],\"ym_timer_trace\":[";
        bool first=true;
        for(const auto& event:b.fm.timer_trace){
            if(!first)std::cout<<',';
            first=false;
            std::cout<<"{\"clock\":"<<event.clock<<",\"register\":"<<unsigned(event.reg)<<",\"value\":"<<unsigned(event.value)<<'}';
        }
        std::cout<<"],\"audio\":\"unavailable\",\"main_cpu_synchronisation\":\"not_implemented\"}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

int z80_component_fixture_main(){try{
    self_test();unsigned cases=0;
    for(unsigned interrupt:{0u,1u,2u})for(unsigned offset:{1u,2u,3u,7u,11u,31u,65u,127u,257u}){
        SoundBoard b;sz::machine_config config;SoundCpu cpu(config);
        const uint8_t loop[]={0xf3,0x31,0x00,0xf7,0x21,0x00,0xf0,0xed,0x56,0xfb,0x34,0x18,0xfd};
        const uint8_t irq[]={0xe5,0x21,0x01,0xf0,0x34,0xe1,0xfb,0xed,0x4d};
        const uint8_t nmi[]={0xe5,0x21,0x02,0xf0,0x34,0xe1,0xed,0x45};
        std::copy(std::begin(loop),std::end(loop),b.rom.begin());
        std::copy(std::begin(irq),std::end(irq),b.rom.begin()+0x38);
        std::copy(std::begin(nmi),std::end(nmi),b.rom.begin()+0x66);
        attach(cpu,b);auto cold=cpu.snapshot();
        for(unsigned i=0;i<200+offset;++i)cpu.run(1);
        if(interrupt)cpu.set_input_line(interrupt==1?0:sz::INPUT_LINE_NMI,sz::ASSERT_LINE);
        auto saved=cpu.snapshot();auto ram=b.ram;
        for(unsigned i=0;i<20000;++i)cpu.run(1);
        auto expected=cpu.snapshot();auto expected_ram=b.ram;
        cpu.restore_component(saved);b.ram=ram;
        if(cpu.snapshot()!=saved)throw std::runtime_error("Z80 component did not restore every field");
        for(unsigned i=0;i<20000;++i)cpu.run(1);
        if(cpu.snapshot()!=expected||b.ram!=expected_ram)throw std::runtime_error("Z80 microstate roundtrip diverged");
        if(!b.ram[0]||(interrupt==1&&!b.ram[1])||(interrupt==2&&!b.ram[2]))throw std::runtime_error("synthetic loop/interrupt handler did not execute");
        cpu.restore_component(cold);if(cpu.snapshot()!=cold||cpu.state_host_total_cycles()!=0)throw std::runtime_error("cold host cycle counter not restored");
        ++cases;
    }
    std::cout<<"{\"status\":\"passed\",\"scope\":\"z80_in_memory_component_only\",\"roundtrip_cases\":"<<cases<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#include "audio_capture.h"
#include "streaming_audio.h"
#include "sound_checkpoint_api.h"
struct CompactRecording {
    uint64_t begin=0;bool recording_enabled=true;std::function<void(int16_t,int16_t)> sink;
    std::vector<std::array<int16_t,2>> frames;
    void emit(SunsetStreamingAudio& mixer){while(mixer.ready()){
        if(recording_enabled&&frames.size()>=2880001)throw std::runtime_error("external diagnostic recording limit");
        auto v=mixer.pop();if(sink)sink(int16_t(v[0]),int16_t(v[1]));if(recording_enabled)frames.push_back({int16_t(v[0]),int16_t(v[1])});
    }}
    void wav(const std::string& path)const{
        std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("cannot write future WAV");
        auto le=[&](uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)f.put(char(v>>(8*i)));};
        f.write("RIFF",4);le(36+frames.size()*4,4);f.write("WAVEfmt ",8);le(16,4);le(1,2);le(2,2);le(48000,4);le(192000,4);le(4,2);le(16,2);f.write("data",4);le(frames.size()*4,4);
        for(auto v:frames){le(uint16_t(v[0]),2);le(uint16_t(v[1]),2);}if(!f)throw std::runtime_error("future WAV write failed");
    }
};

struct DualSound {
    SunsetStreamingAudio mixer;
    CompactRecording recording;
    sz::machine_config config;
    std::vector<uint8_t> sample_config;
    SoundBoard board;
    SoundCpu cpu;
    uint64_t elapsed=0,instructions=0,acks=0,requests=0,max_lead=0;
    bool stepping=false;
    DualSound(std::vector<uint8_t> program,std::vector<uint8_t> samples)
      :sample_config(samples),board(std::move(samples)),cpu(config){
        if(program.size()!=0x10000)throw std::runtime_error("invalid sound program length");
        board.rom=std::move(program);attach(cpu,board);
        board.fm.on_sample=[this](int32_t l,int32_t r){mixer.push(true,{l,r});recording.emit(mixer);};
        board.pcm.on_native_sample([this](int32_t l,int32_t r){mixer.push(false,{l,r});recording.emit(mixer);});
        cpu.irq_vector=[this](){
            ++acks;board.ports.acknowledge_irq();
            cpu.set_input_line(0,sz::CLEAR_LINE);return 0xff;
        };
        cpu.instruction_hook=[this](sz::offs_t){
            ++instructions;
            if(stepping){int consumed=1-cpu.cycles_left();
                if(consumed>0)board.advance(elapsed+uint64_t(consumed)*6);}
        };
    }
};
DualSound* dual_create(std::vector<uint8_t> p,std::vector<uint8_t> v){return new DualSound(std::move(p),std::move(v));}
void dual_destroy(DualSound* s){delete s;}
void dual_advance(DualSound* s,uint64_t target){
    while(s->elapsed<target){
        s->board.advance(s->elapsed);s->stepping=true;
        int used=s->cpu.run(1);s->stepping=false;
        if(used<=0)throw std::runtime_error("sound CPU made no progress");
        s->elapsed+=uint64_t(used)*6;s->board.advance(s->elapsed);
    }
    s->max_lead=std::max(s->max_lead,s->elapsed-target);
}
uint8_t dual_read(DualSound* s,unsigned p){return s->board.pcm.main_read(p);}
void dual_write(DualSound* s,unsigned p,uint8_t v){s->board.pcm.main_write(p,v);}
void dual_irq(DualSound* s){++s->requests;s->board.ports.trigger_irq();s->cpu.set_input_line(0,sz::ASSERT_LINE);}
std::string dual_report(DualSound* s){
    std::ostringstream o;o<<"\"sound_ticks\":"<<s->elapsed<<",\"sound_pc\":"<<s->cpu.pc()
      <<",\"sound_instructions\":"<<s->instructions<<",\"sound_irq_requests\":"<<s->requests
      <<",\"sound_irq_acks\":"<<s->acks<<",\"max_sound_lead_ticks\":"<<s->max_lead
      <<",\"reply_ports\":["<<unsigned(dual_read(s,0))<<','<<unsigned(dual_read(s,1))<<']'
      <<",\"fm_samples\":"<<s->board.fm.samples<<",\"fm_nonzero_samples\":"<<s->board.fm.nonzero_samples
      <<",\"pcm_samples\":"<<s->board.pcm.samples()<<",\"pcm_nonzero_samples\":"<<s->board.pcm.nonzero_samples()
      <<",\"fm_digest\":"<<s->board.fm.digest<<",\"pcm_digest\":"<<s->board.pcm.digest()<<",\"mixed_output_begin\":"<<s->recording.begin<<",\"mixed_output_end\":"<<s->mixer.next;return o.str();
}
void dual_test(){
    audio_capture_test();
    self_test();
    std::vector<uint8_t> p(0x10000);
    // DI; LD A,(FA00); INC A; LD (FA02),A; HALT.
    const uint8_t code[]={0xf3,0x3a,0,0xfa,0x3c,0x32,2,0xfa,0x76};
    std::copy(std::begin(code),std::end(code),p.begin());
    DualSound s(p,std::vector<uint8_t>(0x100000));dual_write(&s,0,0x41);dual_advance(&s,6000);
    require(dual_read(&s,0)==0x42,"response comes from executing Z80 through real PCM ports");
    // IM1 handler acknowledges by CPU vector callback, writes another real reply.
    std::fill(p.begin(),p.end(),0);
    const uint8_t init[]={0xf3,0x31,0,0xf7,0xed,0x56,0xfb,0x76,0x18,0xfd};
    const uint8_t handler[]={0x3e,0x77,0x32,3,0xfa,0xfb,0xed,0x4d};
    std::copy(std::begin(init),std::end(init),p.begin());std::copy(std::begin(handler),std::end(handler),p.begin()+0x38);
    // The real PCM SH1 can assert NMI independently of IRQ. Return correctly
    // from that interrupt rather than running uninitialised fixture bytes.
    p[0x66]=0xed;p[0x67]=0x45;
    DualSound irq(p,std::vector<uint8_t>(0x100000));dual_advance(&irq,600);
    dual_irq(&irq);require(irq.board.ports.irq_held&&irq.acks==0,"IRQ held before CPU ack");
    dual_advance(&irq,6000);
    if(irq.acks!=1||irq.board.ports.irq_held)std::cerr<<dual_report(&irq)<<" held="<<irq.board.ports.irq_held<<'\n';
    require(irq.acks==1&&!irq.board.ports.irq_held,"IRQ cleared once by real CPU acknowledge");
    require(dual_read(&irq,1)==0x77,"real interrupt handler ran");
    auto prior=irq.elapsed;dual_advance(&irq,1);require(irq.elapsed==prior,"no backwards clock when carrying overshoot");
    std::cout<<"{\"status\":\"passed\",\"scope\":\"connected_sound\",\"checks\":"<<checks<<"}\n";
}

void dual_write_wav(DualSound* s,const std::string& path,uint64_t ticks){s->recording.wav(path);}

struct SoundCheckpoint {
    decltype(std::declval<SoundCpu&>().snapshot()) cpu;
    SunsetFm::Snapshot fm;
    StatePcm::Snapshot pcm;
    std::array<uint8_t,0x800> ram;
    std::tuple<std::array<uint8_t,4>,bool,bool,bool,uint64_t,uint64_t> ports;
    std::array<uint64_t,10> host;
    SunsetStreamingAudio::Snapshot mixer;
    std::array<uint64_t,2> rom_identity;
    unsigned schema=1;
};
static uint64_t sound_rom_hash(const std::vector<uint8_t>& bytes){uint64_t h=14695981039346656037ull;for(auto b:bytes)h=(h^b)*1099511628211ull;return h;}
SoundCheckpoint* sound_checkpoint(DualSound* s){
    if(s->stepping)throw std::runtime_error("sound checkpoint inside host execution");
    const auto& p=s->board.ports;
    return new SoundCheckpoint{s->cpu.snapshot(),s->board.fm.snapshot(),s->board.pcm.snapshot(),s->board.ram,
        {p.ports,p.synchronised,p.irq_held,p.nmi_line,p.irq_requests,p.nmi_block_until},
        {s->elapsed,s->instructions,s->acks,s->requests,s->max_lead,s->board.ticks,s->board.reads,s->board.writes,s->board.unmapped_reads,s->board.unmapped_writes},
        s->mixer.snapshot(),{sound_rom_hash(s->board.rom),sound_rom_hash(s->sample_config)},2};
}
void sound_checkpoint_delete(SoundCheckpoint* s){delete s;}
bool sound_checkpoints_equal(const SoundCheckpoint* a,const SoundCheckpoint* b){
    return a->schema==b->schema&&a->cpu==b->cpu&&a->fm==b->fm&&a->pcm==b->pcm&&a->ram==b->ram&&a->ports==b->ports&&a->host==b->host&&a->mixer==b->mixer&&a->rom_identity==b->rom_identity;
}
DualSound* sound_prepare_restore(const DualSound* live,const SoundCheckpoint* s){
    if(s->schema!=2||s->rom_identity!=std::array<uint64_t,2>{sound_rom_hash(live->board.rom),sound_rom_hash(live->sample_config)})throw std::runtime_error("sound checkpoint identity mismatch");
    live->board.fm.validate_component(s->fm);live->board.pcm.validate_component(s->pcm);cpu_boundary::validate_sound(s->cpu);uint64_t elapsed=s->host[0];
    if(elapsed%6||s->cpu.first!=elapsed/6||s->host[5]!=elapsed||std::get<0>(s->fm.host)!=SunsetFm::chip_clock(elapsed)||s->pcm.host[0]!=SunsetFm::chip_clock(elapsed))throw std::runtime_error("sound checkpoint clock mismatch");
    if(s->host[2]>s->host[3]||std::get<1>(s->mixer)+std::get<2>(s->mixer)!=std::get<3>(s->fm.host)+1||std::get<4>(s->mixer)+std::get<5>(s->mixer)!=s->pcm.host[3]+1)throw std::runtime_error("sound checkpoint counters/capture mismatch");
    SunsetStreamingAudio checked;checked.restore(s->mixer);
    auto prepared=std::make_unique<DualSound>(live->board.rom,live->sample_config);
    prepared->board.fm.restore_component(s->fm);prepared->board.pcm.restore_component(s->pcm);prepared->cpu.restore_component(s->cpu);
    prepared->board.ram=s->ram;
    auto& p=prepared->board.ports;std::tie(p.ports,p.synchronised,p.irq_held,p.nmi_line,p.irq_requests,p.nmi_block_until)=s->ports;
    std::tie(prepared->elapsed,prepared->instructions,prepared->acks,prepared->requests,prepared->max_lead,prepared->board.ticks,prepared->board.reads,prepared->board.writes,prepared->board.unmapped_reads,prepared->board.unmapped_writes)=std::tuple(s->host[0],s->host[1],s->host[2],s->host[3],s->host[4],s->host[5],s->host[6],s->host[7],s->host[8],s->host[9]);
    prepared->mixer=checked;prepared->recording.begin=prepared->mixer.next;
    return prepared.release();
}

#include "persistent_checkpoint_api.h"
#include "state_wire.h"

std::vector<uint8_t> sound_checkpoint_pack(const SoundCheckpoint* s){
    state_wire::Writer w;
    state_wire::put(w,s->schema);state_wire::put(w,s->rom_identity);state_wire::put(w,s->cpu);
    state_wire::put(w,s->fm.schema);state_wire::put(w,s->fm.chip_bytes);state_wire::put(w,s->fm.host);state_wire::put(w,s->fm.checksum);
    state_wire::put(w,s->pcm.schema);state_wire::put(w,s->pcm.chip);state_wire::put(w,s->pcm.host);state_wire::put(w,s->pcm.last);state_wire::put(w,s->pcm.checksum);state_wire::put(w,s->pcm.rom_identity);
    state_wire::put(w,s->ram);state_wire::put(w,s->ports);state_wire::put(w,s->host);state_wire::put(w,s->mixer);return std::move(w.bytes);
}
SoundCheckpoint* sound_checkpoint_unpack(std::span<const uint8_t> bytes){
    auto s=std::make_unique<SoundCheckpoint>();state_wire::Reader r(bytes);
    state_wire::get(r,s->schema);state_wire::get(r,s->rom_identity);state_wire::get(r,s->cpu);
    state_wire::get(r,s->fm.schema);state_wire::get(r,s->fm.chip_bytes);state_wire::get(r,s->fm.host);state_wire::get(r,s->fm.checksum);
    state_wire::get(r,s->pcm.schema);state_wire::get(r,s->pcm.chip);state_wire::get(r,s->pcm.host);state_wire::get(r,s->pcm.last);state_wire::get(r,s->pcm.checksum);state_wire::get(r,s->pcm.rom_identity);
    state_wire::get(r,s->ram);state_wire::get(r,s->ports);state_wire::get(r,s->host);state_wire::get(r,s->mixer);r.finish();
    if(s->schema!=2||s->fm.schema!=1||s->pcm.schema!=1)throw std::runtime_error("unsupported sound component schema");return s.release();
}

void compact_clear_recording(DualSound* s){s->recording.frames.clear();s->recording.begin=s->mixer.next;}
void compact_invalid_test(DualSound* live){
    std::unique_ptr<SoundCheckpoint,decltype(&sound_checkpoint_delete)>saved(sound_checkpoint(live),sound_checkpoint_delete);
    auto before=sound_checkpoint_pack(saved.get());
    for(unsigned mode=0;mode<3;++mode){auto bad=*saved;
        if(mode==0)std::get<2>(bad.mixer)=0;
        if(mode==1)std::get<1>(bad.mixer)++;
        if(mode==2)std::get<3>(bad.mixer)[7][0]=1;
        bool rejected=false;try{std::unique_ptr<DualSound,decltype(&dual_destroy)>candidate(sound_prepare_restore(live,&bad),dual_destroy);}catch(const std::runtime_error&){rejected=true;}
        std::unique_ptr<SoundCheckpoint,decltype(&sound_checkpoint_delete)>after(sound_checkpoint(live),sound_checkpoint_delete);
        if(!rejected||sound_checkpoint_pack(after.get())!=before)throw std::runtime_error("bad compact sound changed live state");
    }
}

unsigned guarded_sound_test(DualSound* live){
    auto saved=live->cpu.snapshot();cpu_boundary::validate_sound(saved);unsigned n=0;
    auto reject=[&](const auto&bad){bool caught=false;try{live->cpu.restore_component(bad);}catch(const std::runtime_error&){caught=true;}if(!caught)throw std::runtime_error("invalid Z80 fixture admitted");if(live->cpu.snapshot()!=saved)throw std::runtime_error("bad Z80 state mutated live CPU");++n;};
    for(auto ref:sunset_guard::sound_refs){auto good=saved;std::get<32>(good.second)=ref;cpu_boundary::validate_sound(good);}
    for(uint32_t ref:{uint32_t(0xffffffff),uint32_t(0x123456),uint32_t(0xfffffe)}){auto bad=saved;std::get<32>(bad.second)=ref;reject(bad);}
    auto bad=saved;std::get<18>(bad.second)=2;reject(bad);bad=saved;std::get<19>(bad.second)=3;reject(bad);bad=saved;std::get<27>(bad.second)=128;reject(bad);return n;
}

void ranged_sound_tests(DualSound* s){if(pcm_guard::invalid_tests(s->board.pcm)!=38)throw std::runtime_error("PCM range fixture count");}

void fm_ranged_sound_tests(DualSound* s){if(fm_guard::invalid_tests(s->board.fm)!=214)throw std::runtime_error("FM range fixture count");}

void boundary_sound_tests(DualSound*s){
    auto saved=s->cpu.snapshot();unsigned n=0;
    auto reject=[&](const auto&bad){bool caught=false;try{s->cpu.restore_component(bad);}catch(const std::runtime_error&){caught=true;}if(!caught||s->cpu.snapshot()!=saved)throw std::runtime_error("invalid Z80 boundary changed CPU");++n;};
    auto bad=saved;std::get<33>(bad.second)=1;reject(bad);bad=saved;std::get<33>(bad.second)=-65;reject(bad);
    for(uint8_t value:{uint8_t(0),uint8_t(3),uint8_t(255)}){bad=saved;std::get<34>(bad.second)=value;reject(bad);}
    bad=saved;std::get<35>(bad.second)=0;reject(bad);bad=saved;std::get<36>(bad.second)=0;reject(bad);bad=saved;bad.first=sunset_session::admission_seconds*8000000+1;reject(bad);
    if(n!=8)throw std::runtime_error("Z80 boundary test count");
    std::unique_ptr<SoundCheckpoint,decltype(&sound_checkpoint_delete)>before(sound_checkpoint(s),sound_checkpoint_delete);auto bad_full=*before;std::get<34>(bad_full.cpu.second)=0;
    bool caught=false;try{std::unique_ptr<DualSound,decltype(&dual_destroy)>candidate(sound_prepare_restore(s,&bad_full),dual_destroy);}catch(const std::runtime_error&){caught=true;}
    std::unique_ptr<SoundCheckpoint,decltype(&sound_checkpoint_delete)>after(sound_checkpoint(s),sound_checkpoint_delete);
    if(!caught||!sound_checkpoints_equal(before.get(),after.get()))throw std::runtime_error("invalid Z80 candidate changed sound state");
}

void window_audio_sink(DualSound* s,std::function<void(int16_t,int16_t)> cb){s->recording.frames.clear();s->recording.recording_enabled=false;s->recording.sink=std::move(cb);}

// Rollback's in-memory sound states: the same component checkpoint without the
// ROM fingerprints (a state never leaves this process and is restored only into the sound it came from), and its
// restore in place, without building a second sound board. The file checkpoint and its validated restore are unchanged.
SoundCheckpoint* sound_fast_checkpoint(DualSound* s){
    if(s->stepping)throw std::runtime_error("sound checkpoint inside host execution");
    const auto& p=s->board.ports;
    return new SoundCheckpoint{s->cpu.snapshot(),s->board.fm.snapshot(),s->board.pcm.snapshot(),s->board.ram,
        {p.ports,p.synchronised,p.irq_held,p.nmi_line,p.irq_requests,p.nmi_block_until},
        {s->elapsed,s->instructions,s->acks,s->requests,s->max_lead,s->board.ticks,s->board.reads,s->board.writes,s->board.unmapped_reads,s->board.unmapped_writes},
        s->mixer.snapshot(),{0,0},2};
}
void sound_fast_restore(DualSound* s,const SoundCheckpoint* c){
    if(s->stepping||c->schema!=2)throw std::runtime_error("sound restore inside host execution");
    SunsetStreamingAudio mixer;mixer.restore(c->mixer);
    s->board.fm.restore_component(c->fm);s->board.pcm.restore_component(c->pcm);s->cpu.restore_component(c->cpu);
    s->board.ram=c->ram;
    auto& p=s->board.ports;std::tie(p.ports,p.synchronised,p.irq_held,p.nmi_line,p.irq_requests,p.nmi_block_until)=c->ports;
    std::tie(s->elapsed,s->instructions,s->acks,s->requests,s->max_lead,s->board.ticks,s->board.reads,s->board.writes,s->board.unmapped_reads,s->board.unmapped_writes)=std::tuple(c->host[0],c->host[1],c->host[2],c->host[3],c->host[4],c->host[5],c->host[6],c->host[7],c->host[8],c->host[9]);
    s->mixer=mixer;s->recording.begin=s->mixer.next;s->recording.frames.clear();
}
