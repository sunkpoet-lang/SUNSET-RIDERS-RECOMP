#include "state_tuple.h"
#include "cpu_boundary_guard.h"
// Diagnostic 68000 interpreter probe; not a Sunset Riders game executable.
// Uses unchanged MAME CPU tooling via arcade-decomp (upstream notices retained).
#include "emu.h"
#include "m68000.h"
#include "board_bus.h"
#include <iostream>
#include <iterator>
#include <set>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

class ProbeCpu : public m68000_device {
    static inline machine_config config;
public:
    ProbeCpu():m68000_device(config,"maincpu",nullptr,16000000) {m_icount=0;}
    uint32_t pc() const { return m_ipc; }
    auto snapshot() const {return std::make_tuple(state_own(m_da),state_own(m_ipc),state_own(m_pc),state_own(m_au),state_own(m_at),state_own(m_aob),state_own(m_dt),state_own(m_int_vector),state_own(m_sp),state_own(m_bcount),state_own(m_count_before_instruction_step),state_own(m_t),state_own(m_movems),state_own(m_isr),state_own(m_sr),state_own(m_new_sr),state_own(m_dbin),state_own(m_dbout),state_own(m_edb),state_own(m_irc),state_own(m_ir),state_own(m_ird),state_own(m_ftu),state_own(m_aluo),state_own(m_alue),state_own(m_alub),state_own(m_movemr),state_own(m_irdi),state_own(m_base_ssw),state_own(m_ssw),state_own(m_dcr),state_own(m_virq_state),state_own(m_nmi_pending),state_own(m_int_level),state_own(m_int_next_state),state_own(m_nmi_uses_generic),state_own(m_last_vpa_time),state_own(m_inst_state),state_own(m_inst_substate),state_own(m_next_state),state_own(m_post_run),state_own(m_post_run_cycles),state_own(m_icount));}
    void restore_component(const auto& s){cpu_boundary::validate_main(s);restore_state_tuple(s,std::tie(m_da,m_ipc,m_pc,m_au,m_at,m_aob,m_dt,m_int_vector,m_sp,m_bcount,m_count_before_instruction_step,m_t,m_movems,m_isr,m_sr,m_new_sr,m_dbin,m_dbout,m_edb,m_irc,m_ir,m_ird,m_ftu,m_aluo,m_alue,m_alub,m_movemr,m_irdi,m_base_ssw,m_ssw,m_dcr,m_virq_state,m_nmi_pending,m_int_level,m_int_next_state,m_nmi_uses_generic,m_last_vpa_time,m_inst_state,m_inst_substate,m_next_state,m_post_run,m_post_run_cycles,m_icount));bool supervisor=m_sp==16;
        m_program=supervisor?m_r_program:m_r_uprogram;m_opcodes=supervisor?m_r_opcodes:m_r_uopcodes;
        if(m_mmu)m_mmu->set_super(supervisor);
    }

    void isolated_binding_fixture(){
        auto saved=snapshot();auto rp=m_r_program,ro=m_r_opcodes,up=m_r_uprogram,uo=m_r_uopcodes;
        address_space super_data,super_code,user_data,user_code;
        m_r_program.space=&super_data;m_r_opcodes.space=&super_code;m_r_uprogram.space=&user_data;m_r_uopcodes.space=&user_code;
        for(bool supervisor:{false,true}){auto s=saved;std::get<8>(s)=supervisor?16:15;restore_component(s);
            if(m_program.space!=(supervisor?&super_data:&user_data)||m_opcodes.space!=(supervisor?&super_code:&user_code))throw std::runtime_error("derived memory bindings not restored");}
        m_r_program=rp;m_r_opcodes=ro;m_r_uprogram=up;m_r_uopcodes=uo;restore_component(saved);
        if(snapshot()!=saved)throw std::runtime_error("binding fixture changed CPU fields");
    }

};
static SunsetBus* bus=nullptr;
static bool timed_run=false;
static std::function<void()> sync_time,sync_irq;
static u16 read_bus(offs_t a,u16 m){if(timed_run&&sync_time)sync_time();return bus->read(a,m);}
static void write_bus(offs_t a,u16 d,u16 m){
    if(timed_run&&sync_time)sync_time();
    bus->write(a,d,m);
    if(timed_run&&sync_irq)sync_irq();
}
static u16 ack(offs_t a,u16){return u16(0x18+((a>>1)&7));}
static void attach(ProbeCpu& cpu,SunsetBus& board){
    bus=&board;
    for(int space:{int(AS_PROGRAM),int(AS_OPCODES)}){
        cpu.space(space).read16=read_bus;cpu.space(space).write16=write_bus;
    }
    cpu.space(AS_OPCODES).instructions=true;
    cpu.space(m68000_base_device::AS_CPU_SPACE).read16=ack;
    cpu.start();cpu.reset();
}
static unsigned checks=0;
static void require(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
static void self_test(){
    std::vector<uint8_t> data(0xc0000);
    // Synthetic reset vectors and program: word/byte stores followed by STOP.
    const uint8_t vectors[]={0x00,0x10,0x41,0x00,0,0,1,0};
    std::copy(std::begin(vectors),std::end(vectors),data.begin());
    const uint8_t code[]={0x33,0xfc,0x12,0x34,0x00,0x10,0x40,0x00,
        0x13,0xfc,0x00,0xab,0x00,0x10,0x40,0x02,0x4e,0x72,0x27,0x00};
    std::copy(std::begin(code),std::end(code),data.begin()+0x100);
    SunsetBus board(data);
    board.write(0x104000,0x1234);
    require(board.read(0x104000)==0x1234,"big endian word");
    board.write(0x104000,0xab00,0xff00);
    require(board.read(0x104000)==0xab34,"upper byte mask");
    board.write(0x104000,0x00cd,0x00ff);
    require(board.read(0x104000)==0xabcd,"lower byte mask");
    board.write(0x107ffe,0x5678);require(board.read(0x107ffe)==0x5678,"RAM final word");
    board.write(0x140000,0x6bad);require(board.read(0x140000)==0x6bad,"palette backing RAM");
    board.write(0x1c057e,0x1357);require(board.read(0x1c057e)==0x1357,"auxiliary end");
    board.write(0,0xffff);require(board.rom==data,"ROM remains immutable");
    bool stopped=false;try{board.read(0x1c0802);}catch(const BusStop& s){stopped=std::string(s.what())=="device_not_implemented";}
    require(stopped,"pending device must stop");
    stopped=false;try{board.read(0x108000);}catch(const BusStop& s){stopped=std::string(s.what())=="unmapped_access";}
    require(stopped,"RAM overrun must stop");
    stopped=false;try{board.read(0x104001);}catch(const BusStop& s){stopped=std::string(s.what())=="unaligned_word";}
    require(stopped,"odd word must stop");
    auto old_ram=board.ram;
    board.write(0x60b800,0x1234);
    require(board.unmapped_writes.at(0x60b800)==1&&board.ram==old_ram,"unmapped write logs without mutation");
    board.write(0x1c0800,0);
    require(board.protection_writes==1,"protection offset zero is mapped no-op");
    board.write(0x1104000,0x1122);require(board.read(0x104000)==0x1122,"24-bit bus wrap");
    board.write(0x104000,0xffff,0);require(board.read(0x104000)==0x1122,"zero write mask");
    require(!board.watchdog.armed,"watchdog cold reset unarmed");
    require(board.read(0x1c0400)==0&&board.watchdog.feeds==1,"watchdog read feeds and returns map default");
    board.read(0x1c0400,0xffff,false);require(board.watchdog.feeds==1,"debug read must not feed watchdog");
    board.watchdog.advance(SunsetWatchdog::timeout_ticks-1);
    require(!board.watchdog.soft_reset_requested,"watchdog not expired early");
    board.write(0x1c0400,0x1234);require(board.watchdog.feeds==2,"watchdog write feed");
    board.watchdog.advance(SunsetWatchdog::timeout_ticks);
    require(!board.watchdog.soft_reset_requested,"watchdog feed postpones deadline");
    board.watchdog.advance(2*SunsetWatchdog::timeout_ticks-1);
    require(board.watchdog.soft_reset_requested&&!board.watchdog.armed,"watchdog exact deadline reset request");
    bool rejected=false;try{board.watchdog.advance(0);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"watchdog backwards time rejected");
    require(board.tiles.irq_control==0&&!board.tiles.irq,"tile IRQ cold state");
    board.write(0x600000,0x1234);
    require(board.tiles.ram[0]==0x12&&board.tiles.ram[0x2000]==0,"word write only drives MSB bank");
    board.write(0x600000,0x00ab,0x00ff);
    require(board.tiles.ram[0]==0x12&&board.tiles.ram[0x2000]==0xab,"low byte uses chip A13 bank");
    require(board.read(0x600000)==0x1200,"word read high bank precedence");
    require(board.read(0x600000,0x00ff)==0xab,"low read bank mapping");
    board.write(0x603a00,0x0400);
    require(board.tiles.irq_control==4&&!board.tiles.irq,"IRQ enable does not assert immediately");
    board.tiles.vblank(true);require(board.tiles.irq,"enabled VBLANK asserts IRQ");
    board.tiles.vblank(false);require(board.tiles.irq,"falling VBLANK does not acknowledge IRQ");
    board.write(0x603a00,0);
    require(!board.tiles.irq&&board.tiles.irq_control==0,"IRQ disable clears asserted line");
    board.tiles.vblank(true);require(!board.tiles.irq,"disabled IRQ stays clear on VBLANK");
    board.write(0x603b00,0xa500);board.write(0x603e00,0x3c00);
    require(board.tiles.banks[0]==5&&board.tiles.banks[1]==10&&board.tiles.banks[2]==12&&board.tiles.banks[3]==3,"character bank nibbles");
    board.write(0x603c00,0x9a00);board.write(0x603d00,0xff00);
    require(board.tiles.romsubbank==0x9a&&board.tiles.tileflip==7,"ROM subbank and flip mask");
    board.write(0x603800,0x8100);board.write(0x603900,0x4200);
    require(board.tiles.addrmap==0x81&&board.tiles.scrollctrl==0x42,"address and scroll controls retained");
    board.write(0x603a00,0xffff,0);
    require(board.tiles.irq_control==0,"zero mask leaves tile state untouched");
    board.tiles.rmrd=true;
    stopped=false;try{board.read(0x600000);}catch(const BusStop& s){stopped=std::string(s.what())=="tile_rom_read_not_implemented";}
    require(stopped,"unsupported character ROM read stops explicitly");
    board.tiles.rmrd=false;
    // Serial golden scenarios use synthetic EEPROM bytes, never player saves.
    std::vector<uint8_t> seed(128,0xff);seed[5]=0xa6;
    SunsetEeprom ep(seed);uint64_t tick=0;
    auto line=[&](bool di,bool cs,bool clk){ep.lines(di,cs,clk,++tick);};
    auto bit=[&](bool value){line(value,true,false);line(value,true,true);};
    auto command=[&](unsigned opcode,unsigned address){
        line(false,false,false);line(false,true,false);bit(true);
        unsigned value=(opcode<<9)|address;
        for(int i=10;i>=0;--i)bit((value>>i)&1);
    };
    require(ep.locked&&ep.ready()&&ep.output(),"EEPROM cold locked/ready/pullup");
    command(2,5);require(!ep.output(),"EEPROM READ dummy zero");
    unsigned value=0;
    for(int i=0;i<8;++i){bit(false);value=(value<<1)|unsigned(ep.output());}
    require(value==0xa6,"EEPROM serial READ MSB-first");
    command(1,5);for(int i=7;i>=0;--i)bit((0x12>>i)&1);
    require(ep.data[5]==0xa6&&ep.ready(),"EEPROM locked WRITE rejected");
    command(0,0x180);require(!ep.locked,"EEPROM UNLOCK nine address bits");
    command(3,5);for(int i=7;i>=0;--i)bit((0x39>>i)&1);
    require(ep.data[5]==0x39&&!ep.ready(),"ER5911 opcode11 WRITE and busy");
    auto deadline=ep.busy_until;ep.advance(deadline-1);
    require(!ep.ready(),"EEPROM busy before write deadline");
    ep.advance(deadline);tick=deadline;require(ep.ready(),"EEPROM ready at 1750us deadline");
    command(0,0);require(ep.locked,"EEPROM LOCK");
    command(0,0x180);command(0,0x100);
    require(ep.data[5]==0xff&&!ep.ready(),"EEPROM unlocked ERASEALL and busy");
    ep.advance(ep.busy_until);tick=ep.now;require(ep.ready(),"EEPROM erase all completes at 8ms");
    command(2,5);line(false,false,false);
    require(ep.state==SunsetEeprom::Reset&&ep.output(),"EEPROM CS abort restores pullup");
    SunsetEeprom simultaneous(seed);
    simultaneous.lines(true,true,true,1);
    require(simultaneous.state==SunsetEeprom::Start,"CS+CLK same timestamp ignores false start");
    auto previous_feeds=board.eeprom_control_writes;
    board.write(0x1c0200,0x3f00,0xff00);
    require(board.eeprom_control_writes==previous_feeds,"EEPROM control ignores high byte");
    board.write(0x1c0200,0x0038,0x00ff);
    require(board.dim_control==0x18&&board.sprite_rom_bank==4,"EEPROM control dim/sprite latches");
    require(board.priority.priority(0)==0&&board.priority.palette(4)==0,"priority cold state");
    board.write(0x5c0700,0xffff,0xff00);
    require(board.priority.writes==0,"priority ignores high byte");
    board.write(0x5c0700,0xffaa);
    require(board.priority.priority(0)==0x2a,"priority low byte six-bit mask");
    board.write(0x5c0712,0x0039);
    require(board.priority.palette(0)==32&&board.priority.palette(1)==64&&board.priority.palette(2)==96,"CI0-CI2 palette bases");
    board.write(0x5c0714,0x002b);
    require(board.priority.palette(3)==48&&board.priority.palette(4)==80,"CI3-CI4 palette bases");
    board.priority.write(0x10,0x5f);
    require(board.priority.priority(0)==0x1f,"priority register address mirror");
    auto priority_writes=board.priority.writes;
    board.write(0x5c0700,0,0);
    require(board.priority.writes==priority_writes,"zero mask leaves priority untouched");
    stopped=false;try{board.read(0x5c0700);}catch(const BusStop&){stopped=true;}
    require(stopped,"priority bus is write-only");
    board.write(0x1c0300,1);
    require(board.coin_lines[0]&&board.coin_counts[0]==0,"coin edge at time zero is not counted");
    board.eeprom.advance(3);board.write(0x1c0300,1);
    require(board.coin_counts[0]==0,"held initial coin is not counted later");
    board.write(0x1c0300,0);board.write(0x1c0300,1);
    require(board.coin_counts[0]==1,"coin rising edge increments once");
    board.write(0x1c0300,1);require(board.coin_counts[0]==1,"held coin does not repeat");
    board.write(0x1c0300,3);
    require(board.coin_counts[0]==1&&board.coin_counts[1]==1,"independent second coin edge");
    auto controls=board.board_control_writes;
    board.write(0x1c0300,0,0xff00);
    require(board.coin_lines[0]&&board.board_control_writes==controls,"control ignores upper byte");
    board.write(0x1c0300,0x78);
    require(board.tiles.rmrd&&board.dim_video==7,"control enables ROM read and dim bits");
    stopped=false;try{board.read(0x600000);}catch(const BusStop& s){stopped=std::string(s.what())=="tile_rom_read_not_implemented";}
    require(stopped,"enabled but unimplemented ROM path still stops");
    board.write(0x1c0300,0x20);
    require(!board.tiles.rmrd&&board.dim_video==2,"control clears ROM mode independently of dim bits");
    controls=board.board_control_writes;
    board.write(0x1c0300,0xffff,0);
    require(board.board_control_writes==controls&&board.dim_video==2,"control zero mask no mutation");
    board.write(0x5a0008,0x1234);
    require(board.sprites.regs[4]==0x12&&board.sprites.regs[5]==0x34,"sprite control high and low bytes");
    board.write(0x5a000a,0x5678);
    require(board.sprites.regs[4]==0x56&&board.sprites.regs[5]==0x78,"sprite no-A1 mirror");
    board.write(0x5a0008,0xab00,0xff00);
    require(board.sprites.regs[4]==0xab&&board.sprites.regs[5]==0x78,"sprite control upper mask");
    board.write(0x5a0008,0x00cd,0x00ff);
    require(board.sprites.regs[4]==0xab&&board.sprites.regs[5]==0xcd,"sprite control lower mask");
    require(board.read(0x5a0008)==0,"sprite unknown control read returns zero not latch");
    board.write(0x180000,0x1122);board.write(0x180004,0x3344);board.write(0x180080,0x5566);
    require(board.sprites.ram[0]==0x1122&&board.sprites.ram[1]==0x3344&&board.sprites.ram[8]==0x5566,"scattered chip RAM wiring golden");
    board.write(0x180002,0x7788);
    require(board.sprites.shared[1]==0x7788&&board.sprites.ram[0]==0x1122,"disconnected sprite RAM remains CPU backing only");
    board.sprites.ram[1]=0x9876;
    require(board.read(0x180004)==0x9876&&board.sprites.shared[2]==0x3344,"connected read uses chip RAM");
    board.write(0x180004,0xab00,0xff00);
    require(board.read(0x180004)==0xab76,"scattered RAM combines chip bytes independently");
    board.write(0x183f9c,0xa1b2);
    require(board.sprites.ram.back()==0xa1b2,"scattered final connected word");
    board.write(0x5a000c,0,0xff00);
    require(board.sprites.buffer[0]==0x1122&&board.sprites.buffer[1]==0xab76,"buffer update trigger copies chip RAM");
    board.write(0x5a000c,0,0x00ff);
    require(board.sprites.buffer[0]==0&&board.sprites.buffer[1]==0xab76,"buffer clear only resets first word of each sprite");
    board.sprites.ram[0]=0x8011;
    board.write(0x5a000c,0);
    require(board.sprites.buffer[0]==0&&board.sprites.buffer[1]==0xab76,"word write update then clear ordered");
    auto updates=board.sprites.buffer_updates,clears=board.sprites.buffer_clears;
    board.read(0x5a000c,0xffff,false);
    require(board.sprites.buffer_updates==updates&&board.sprites.buffer_clears==clears,"debug register reads suppress buffer side effects");
    stopped=false;try{board.read(0x5a000c);}catch(const BusStop& s){stopped=std::string(s.what())=="sprite_pair_read_order_unverified";}
    require(stopped,"ambiguous paired read trigger order stops");
    board.sprites.rom.resize(0x200000);
    board.sprites.rom_bank=0;board.sprites.regs[8]=0;board.sprites.regs[9]=0;board.sprites.regs[11]=0;
    board.sprites.rom[0]=0x10;board.sprites.rom[1]=0x20;board.sprites.rom[2]=0x30;board.sprites.rom[3]=0x40;
    require(board.read(0x5a0018)==0x2010&&board.read(0x5a001c)==0x4030,"sprite ROM lane XOR golden");
    require(board.read(0x5a001a)==0x2010,"sprite ROM register mirror");
    SunsetBus timed(data);
    require(timed.read(0x1c0102)==0xf3,"cold status DO/READY and inverted VBLANK");
    timed.advance(49151);require(timed.status()==0xf3,"VBLANK before visible line16");
    timed.advance(49152);require(timed.status()==0xfb,"visible line16 clears VBLANK");
    timed.tiles.write(0x1d00,4);
    timed.advance(737279);require(!timed.vblank()&&!timed.tiles.irq,"no early VBLANK or IRQ");
    timed.advance(737280);require(timed.vblank()&&timed.tiles.irq,"line240 VBLANK and enabled IRQ");
    timed.advance(811008);require(timed.vblank()&&timed.vblank_entries==1,"frame wrap remains blank without extra entry");
    timed.advance(811008+49152);
    require(!timed.vblank()&&timed.vblank_exits==2,"next frame visible boundary");
    timed.service_test=true;require((timed.status()&0x80)==0,"service test active low");
    timed.eeprom.busy_until=timed.master_ticks+10;
    require((timed.status()&2)==0,"status propagates EEPROM busy");
    timed.advance(timed.master_ticks+10);require(timed.status()&2,"board time completes EEPROM busy");
    rejected=false;try{timed.advance(0);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"backwards board time rejected");
    SunsetBus expiry(data);expiry.read(0x1c0400);
    stopped=false;try{expiry.advance(SunsetWatchdog::timeout_ticks);}catch(const BusStop& s){stopped=std::string(s.what())=="watchdog_soft_reset_pending";}
    require(stopped,"board clock exposes watchdog soft-reset requirement");
    board.write(0x5c0600,0xff34,0xff00);
    require(board.sound.ports[0]==0,"sound port high byte ignored");
    board.write(0x5c0600,0xab34);board.write(0x5c0602,0x0056);
    require(board.sound.sound_read(0)==0x34&&board.sound.sound_read(1)==0x56,"main command ports low bytes");
    stopped=false;try{board.read(0x5c0600);}catch(const BusStop& s){stopped=std::string(s.what())=="sound_cpu_not_synchronised";}
    require(stopped,"main cannot read a fake unsynchronised sound response");
    require(board.read(0x5c0600,0xff00)==0,"unwired high-byte sound read");
    board.sound.synchronised=true;board.sound.sound_write(2,0xa9);board.sound.sound_write(3,0x8b);
    require(board.read(0x5c0600)==0xa9&&board.read(0x5c0602)==0x8b,"main response port mapping");
    board.write(0x5c0604,0x1234);require(board.sound.irq_held,"main sound trigger holds IRQ");
    board.sound.synchronised=false;
    ProbeCpu cpu;attach(cpu,board);
    const int used=cpu.run(1000);
    require(used>=1000,"bounded CPU budget");
    require(board.read(0x104000)==0x1234,"CPU word store");
    require(board.read(0x104002)==0xab00,"CPU byte store lane");
    std::cout<<"{\"status\":\"passed\",\"scope\":\"synthetic_bus_and_cpu\",\"checks\":"<<checks<<"}\n";
}
int independent_probe_main(int argc,char** argv){
    try{
        if(argc==2&&std::string(argv[1])=="--self-test"){self_test();return 0;}
        if(argc==2&&std::string(argv[1])=="--priority-vectors"){
            SunsetPriority chip;unsigned offset,value;
            while(std::cin>>offset>>value){
                chip.write(offset,uint8_t(value));
                for(unsigned i=0;i<16;++i)std::cout<<unsigned(chip.priority(i))<<' ';
                for(unsigned i=0;i<5;++i)std::cout<<unsigned(chip.palette(i))<<' ';
                std::cout<<'\n';
            }
            return 0;
        }
        if(argc!=2)throw std::runtime_error("usage: cpu_probe CYCLE_LIMIT < private_program_pipe");
        size_t end=0;const int cycles=std::stoi(argv[1],&end);
        if(end!=std::string(argv[1]).size()||cycles<1||cycles>16000000)throw std::runtime_error("invalid cycle limit");
#ifdef _WIN32
        _setmode(_fileno(stdin),_O_BINARY);
#endif
        std::vector<uint8_t> data(0xc0000);
        std::cin.read(reinterpret_cast<char*>(data.data()),data.size());
        if(std::cin.gcount()!=std::streamsize(data.size()))throw std::runtime_error("expected program on stdin");
        std::vector<uint8_t> initial_eeprom(128);
        std::cin.read(reinterpret_cast<char*>(initial_eeprom.data()),initial_eeprom.size());
        if(std::cin.gcount()!=128)throw std::runtime_error("expected 128 EEPROM bytes after program");
        SunsetBus board(std::move(data),initial_eeprom);ProbeCpu cpu;
        board.sprites.rom.resize(0x200000);
        std::cin.read(reinterpret_cast<char*>(board.sprites.rom.data()),board.sprites.rom.size());
        if(std::cin.gcount()!=0x200000||std::cin.peek()!=EOF)throw std::runtime_error("expected sprite ROM after EEPROM");
        uint64_t instructions=0;std::set<uint32_t> addresses;
        cpu.instruction_hook=[&](offs_t pc){
            ++instructions;addresses.insert(pc);
            if(timed_run&&sync_time)sync_time();
        };
        bool irq_line=false;
        sync_irq=[&](){if(irq_line!=board.tiles.irq){irq_line=board.tiles.irq;cpu.set_input_line(4,irq_line?ASSERT_LINE:CLEAR_LINE);}};
        sync_time=[&](){
            int elapsed=cycles-cpu.cycles_left();
            if(elapsed>=0)board.advance(uint64_t(elapsed)*3);
            sync_irq();
        };
        try{
            attach(cpu,board);timed_run=true;int used=cpu.run(cycles);sync_time();timed_run=false;
            std::cout<<"{\"status\":\"diagnostic_budget_exhausted\",\"cycles_consumed\":"<<used;
        }catch(const BusStop& stop){
            timed_run=false;
            std::cout<<"{\"status\":\"diagnostic_incomplete\",\"reason\":\""<<stop.what()
             <<"\",\"address\":"<<stop.address<<",\"write\":"<<(stop.write?"true":"false");
        }
        std::cout<<",\"pc\":"<<cpu.pc()<<",\"instructions\":"<<instructions<<",\"unique_pcs\":"<<addresses.size()
         <<",\"bus_reads\":"<<board.reads<<",\"bus_writes\":"<<board.writes<<",\"unmapped_writes\":[";
        bool first=true;
        for(const auto& [address,count]:board.unmapped_writes){
            if(!first)std::cout<<',';
            first=false;
            std::cout<<"{\"address\":"<<address<<",\"count\":"<<count<<'}';
        }
        std::cout<<"],\"watchdog_feeds\":"<<board.watchdog.feeds
         <<",\"tile_writes\":"<<board.tiles.writes<<",\"tile_irq_control\":"<<unsigned(board.tiles.irq_control)
         <<",\"eeprom_control_writes\":"<<board.eeprom_control_writes<<",\"eeprom_commands\":"<<board.eeprom.commands
         <<",\"eeprom_data_writes\":"<<board.eeprom.writes<<",\"eeprom_time\":\"instruction_boundary_approximation\""
         <<",\"priority_writes\":"<<board.priority.writes
         <<",\"board_control_writes\":"<<board.board_control_writes<<",\"tile_rom_read_enabled\":"<<(board.tiles.rmrd?"true":"false")
         <<",\"dim_video\":"<<unsigned(board.dim_video)<<",\"coin_counts\":["<<board.coin_counts[0]<<','<<board.coin_counts[1]<<']'
         <<",\"sprite_register_writes\":"<<board.sprites.register_writes<<",\"sprite_ram_writes\":"<<board.sprites.ram_writes
         <<",\"sprite_rom_reads\":"<<board.sprites.rom_reads<<",\"sprite_buffer_updates\":"<<board.sprites.buffer_updates
         <<",\"master_ticks\":"<<board.master_ticks<<",\"vblank_entries\":"<<board.vblank_entries
         <<",\"vblank_exits\":"<<board.vblank_exits<<",\"status_reads\":"<<board.status_reads
         <<",\"time_integration\":\"cpu_cycle_stub_approximation\",\"watchdog_time_integration\":\"clocked_reset_handling_pending\""
         <<",\"game_boot\":\"not_validated\",\"video\":\"unavailable\",\"audio\":\"unavailable\"}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

int main(){try{
    self_test();unsigned tests=0;
    for(bool interrupt:{false,true})for(int offset:{1,2,3,7,11,31,65,127,257}){
        std::vector<uint8_t> rom(0xc0000);
        const uint8_t vectors[]={0x00,0x10,0x7f,0xf0,0,0,1,0};std::copy(std::begin(vectors),std::end(vectors),rom.begin());
        const uint8_t loop[]={0x46,0xfc,0x20,0x00,0x52,0x79,0x00,0x10,0x40,0x00,0x60,0xf8};
        std::copy(std::begin(loop),std::end(loop),rom.begin()+0x100);
        rom[0x72]=2; // Vector0x1c points at handler0x200.
        const uint8_t handler[]={0x52,0x79,0x00,0x10,0x40,0x02,0x4e,0x73};
        std::copy(std::begin(handler),std::end(handler),rom.begin()+0x200);
        SunsetBus board(std::move(rom));ProbeCpu cpu;attach(cpu,board);
        for(int i=0;i<200+offset;++i)cpu.run(1);
        if(interrupt)cpu.set_input_line(4,ASSERT_LINE);
        auto saved=cpu.snapshot();auto memory=board.ram;
        for(unsigned i=0;i<20000;++i)cpu.run(1);
        auto expected=cpu.snapshot();auto expected_ram=board.ram;
        cpu.restore_component(saved);board.ram=memory;
        if(cpu.snapshot()!=saved)throw std::runtime_error("CPU component restore lost a field");
        for(unsigned i=0;i<20000;++i)cpu.run(1);
        if(cpu.snapshot()!=expected||board.ram!=expected_ram)throw std::runtime_error("CPU microstate roundtrip diverged");
        if(board.read(0x104000)==0)throw std::runtime_error("synthetic main loop did not execute");
        if(interrupt&&board.read(0x104002)==0)throw std::runtime_error("level4 handler did not execute");
        ++tests;
    }
    std::cout<<"{\"status\":\"passed\",\"scope\":\"68000_in_memory_component_only\",\"roundtrip_cases\":"<<tests<<"}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#include "board_state.h"
int boundary_cpu_fixture(){try{
    std::vector<uint8_t> rom(0xc0000);const uint8_t vectors[]={0,0x10,0x7f,0xf0,0,0,1,0};std::copy(std::begin(vectors),std::end(vectors),rom.begin());
    const uint8_t code[]={0x4e,0x71,0x60,0xfc};std::copy(std::begin(code),std::end(code),rom.begin()+0x100);
    SunsetBus b(std::move(rom));ProbeCpu cpu;attach(cpu,b);auto cold=cpu.snapshot();cpu_boundary::validate_main(cold);auto initial=board_snapshot(b);
    for(unsigned i=0;i<1000;++i)cpu.run(1);auto expected=cpu.snapshot();auto memory=b.ram;
    board_restore(b,initial);cpu.restore_component(cold);if(cpu.snapshot()!=cold)throw std::runtime_error("cold CPU restore lost fields");
    for(unsigned i=0;i<1000;++i)cpu.run(1);if(cpu.snapshot()!=expected||b.ram!=memory)throw std::runtime_error("cold CPU continuation diverged");
    cpu.isolated_binding_fixture();std::cout<<"{\"status\":\"passed\",\"scope\":\"cold_main_and_derived_bindings\",\"cold_roundtrips\":1,\"binding_cases\":2}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
