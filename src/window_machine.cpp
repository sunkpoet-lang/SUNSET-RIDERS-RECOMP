// Typed in-process checkpoint experiment; no public save-state file interface.
#define main main_cpu_component_fixture
#include "boundary_cpu_probe.cpp"
#undef main
#include "sound_checkpoint_api.h"
#include "video_checkpoint_api.h"
#include "board_state.h"
#include "scenario_inputs.h"
#include <memory>
using SoundOwner=std::unique_ptr<DualSound,decltype(&dual_destroy)>;
using VideoOwner=std::unique_ptr<SunsetVideo,decltype(&video_destroy)>;
using SoundState=std::unique_ptr<SoundCheckpoint,decltype(&sound_checkpoint_delete)>;
using VideoState=std::unique_ptr<VideoCheckpoint,decltype(&video_checkpoint_delete)>;
struct MachineCheckpoint {
    decltype(std::declval<ProbeCpu&>().snapshot()) cpu;
    SunsetBoardSnapshot board;
    SoundState sound;
    VideoState video;
    std::array<uint64_t,8> host;
    uint64_t scenario_identity;
    unsigned schema=1;
};
static bool machine_states_equal(const MachineCheckpoint& a,const MachineCheckpoint& b){
    return a.schema==b.schema&&a.scenario_identity==b.scenario_identity&&a.cpu==b.cpu&&a.board==b.board&&a.host==b.host&&sound_checkpoints_equal(a.sound.get(),b.sound.get())&&video_checkpoints_equal(a.video.get(),b.video.get());
}
struct ExperimentMachine {
    SunsetBus board;
    ProbeCpu cpu;
    SoundOwner sound{nullptr,dual_destroy};
    VideoOwner video{nullptr,video_destroy};
    std::vector<SunsetInputEvent> events;std::function<void(uint64_t)> live_input;std::function<void()> before_frame;
    uint64_t elapsed=0,instructions=0,max_main_lead=0,next_frame_tick=0,frame=0,drawn=0,event=0,scenario_identity=0;
    bool stepping=false,irq_line=false;
    ExperimentMachine(std::vector<uint8_t> p,std::vector<uint8_t> e,std::vector<uint8_t> c,std::vector<uint8_t> m,std::vector<uint8_t> v,std::vector<uint8_t> t,std::vector<SunsetInputEvent> inputs)
      :board(std::move(p),std::move(e)),events(std::move(inputs)){
        board.sprites.rom=std::move(c);sound.reset(dual_create(std::move(m),std::move(v)));video.reset(video_create(std::move(t),board.sprites.rom));
        scenario_identity=14695981039346656037ull;
        auto hash=[&](uint64_t value){for(unsigned i=0;i<8;++i)scenario_identity=(scenario_identity^uint8_t(value>>(8*i)))*1099511628211ull;};
        for(const auto& x:events){hash(x.frame);for(auto h:x.held)hash(h);hash(x.coins);hash(x.services);}
        board.video_edge_callback=[this](bool entering){video_edge(video.get(),board,entering);if(entering)++drawn;};
        board.connected_sound_read=[this](unsigned p){return dual_read(sound.get(),p);};
        board.connected_sound_write=[this](unsigned p,uint8_t v){dual_write(sound.get(),p,v);};
        board.connected_sound_irq=[this](){dual_irq(sound.get());};
        timed_run=false;attach(cpu,board);
        sync_time=[this](){sync();};sync_irq=[this](){update_irq();};
        cpu.instruction_hook=[this](offs_t){++instructions;if(timed_run)sync();};
    }
    void update_irq(){if(irq_line!=board.tiles.irq){irq_line=board.tiles.irq;cpu.set_input_line(4,irq_line?ASSERT_LINE:CLEAR_LINE);}}
    void sync(){
        int consumed=stepping?1-cpu.cycles_left():0;
        uint64_t now=elapsed+uint64_t(std::max(consumed,0))*3;
        if(now<board.master_ticks){max_main_lead=std::max(max_main_lead,board.master_ticks-now);now=board.master_ticks;}
        while(next_frame_tick<=now){
            board.advance(next_frame_tick);dual_advance(sound.get(),next_frame_tick);
            if(before_frame)before_frame();if(live_input)live_input(frame);else if(event<events.size()&&events[event].frame==frame){const auto& e=events[event++];board.inputs.frame(e.held,e.coins,e.services);}
            ++frame;next_frame_tick+=SunsetBus::frame_ticks;
        }
        board.advance(now);dual_advance(sound.get(),now);update_irq();
    }
    void run_until(uint64_t target){
        if(target<elapsed||target>sunset_session::max_master)throw std::runtime_error("invalid experiment target");
        timed_run=true;
        try{while(elapsed<target){sync();stepping=true;int used=cpu.run(1);stepping=false;
            if(used<=0)throw std::runtime_error("main CPU made no progress");elapsed+=uint64_t(used)*3;sync();}}
        catch(...){timed_run=false;stepping=false;throw;}
        timed_run=false;
    }
    MachineCheckpoint checkpoint(){
        if(stepping||timed_run)throw std::runtime_error("checkpoint inside host slice");
        return {cpu.snapshot(),board_snapshot(board),SoundState(sound_checkpoint(sound.get()),sound_checkpoint_delete),VideoState(video_checkpoint(video.get(),board),video_checkpoint_delete),
            {elapsed,instructions,max_main_lead,next_frame_tick,frame,event,drawn,uint64_t(irq_line)},scenario_identity,1};
    }
    void restore(const MachineCheckpoint& s){
        cpu_boundary::validate_main(s.cpu);
        if(stepping||timed_run||s.schema!=1||s.scenario_identity!=scenario_identity)throw std::runtime_error("invalid checkpoint schema/profile/boundary");
        if(s.host[0]%3||s.host[5]>events.size()||s.host[7]>1||s.host[3]!=s.host[4]*SunsetBus::frame_ticks)throw std::runtime_error("invalid checkpoint scheduler");
        auto clock=board_state_value<uint64_t>(s.board,"master_ticks");
        if(clock<s.host[0]||clock-s.host[0]>s.host[2]||s.host[3]<=clock)throw std::runtime_error("checkpoint clock/debt mismatch");
        SunsetBus scratch(board.rom);scratch.sprites.rom=board.sprites.rom;
        board_restore(scratch,s.board);
        SoundOwner prepared_sound(sound_prepare_restore(sound.get(),s.sound.get()),dual_destroy);
        VideoOwner prepared_video(video_prepare_restore(video.get(),s.video.get(),scratch),video_destroy);
        // All candidates validated independently. Board does all allocations/checks
        // before mutating; after that only typed scalar assignments and owner swaps.
        board_restore(board,s.board);cpu.restore_component(s.cpu);
        elapsed=s.host[0];instructions=s.host[1];max_main_lead=s.host[2];next_frame_tick=s.host[3];frame=s.host[4];event=s.host[5];drawn=s.host[6];irq_line=s.host[7]!=0;
        sound.swap(prepared_sound);video.swap(prepared_video);
    }
    void dump(const std::string& prefix){video_write_ppm(video.get(),prefix+".ppm");dual_write_wav(sound.get(),prefix+".wav",board.master_ticks);}
    void report(){std::cout<<"\"main_ticks\":"<<board.master_ticks<<",\"main_pc\":"<<cpu.pc()<<",\"main_instructions\":"<<instructions
        <<",\"scenario_events_applied\":"<<event<<",\"sprite_buffer_updates\":"<<board.sprites.buffer_updates<<",\"sprite_ram_writes\":"<<board.sprites.ram_writes
        <<",\"tile_writes\":"<<board.tiles.writes<<",\"protection_reads\":"<<board.protection_reads<<",\"max_main_lookahead_ticks\":"<<max_main_lead
        <<','<<video_report(video.get())<<','<<dual_report(sound.get());}
};

#include "persistent_checkpoint_api.h"
#include "state_file.h"
#include "state_codec_tests.h"
void compact_clear_recording(DualSound*);
void compact_invalid_test(DualSound*);
unsigned guarded_sound_test(DualSound*);
void ranged_sound_tests(DualSound*);
void fm_ranged_sound_tests(DualSound*);
static void guarded_actual_test(ExperimentMachine&machine){
    auto saved=machine.checkpoint();unsigned n=0;
    auto reject=[&](const auto&bad){bool caught=false;try{machine.cpu.restore_component(bad);}catch(const std::runtime_error&){caught=true;}if(!caught||machine.cpu.snapshot()!=saved.cpu)throw std::runtime_error("bad 68000 state mutated live CPU");++n;};
    auto bad=saved.cpu;std::get<37>(bad)=65535;reject(bad);bad=saved.cpu;std::get<38>(bad)=65535;reject(bad);bad=saved.cpu;std::get<8>(bad)=17;reject(bad);bad=saved.cpu;std::get<33>(bad)=8;reject(bad);bad=saved.cpu;std::get<39>(bad)=0xffffffff;reject(bad);bad=saved.cpu;std::get<40>(bad)=2;reject(bad);
    for(auto pair:sunset_guard::main_pairs){auto good=saved.cpu;std::get<37>(good)=pair>>16;std::get<38>(good)=pair&65535;cpu_boundary::validate_main(good);}
    for(unsigned state=0;state<sunset_guard::main_handlers;++state){auto bad=saved.cpu;std::get<37>(bad)=state;std::get<38>(bad)=65535;bool rejected=false;try{cpu_boundary::validate_main(bad);}catch(const std::runtime_error&){rejected=true;}if(!rejected)throw std::runtime_error("invalid substate boundary admitted");}
    {auto bad=saved.cpu;std::get<37>(bad)=sunset_guard::main_handlers;bool rejected=false;try{cpu_boundary::validate_main(bad);}catch(const std::runtime_error&){rejected=true;}if(!rejected)throw std::runtime_error("one-past-last main handler admitted");}
    auto old=saved.cpu;std::get<37>(saved.cpu)=65535;bool caught=false;try{machine.restore(saved);}catch(const std::runtime_error&){caught=true;}saved.cpu=old;
    if(!caught||!machine_states_equal(saved,machine.checkpoint()))throw std::runtime_error("bad main preflight changed compound machine");
    if(n!=6||guarded_sound_test(machine.sound.get())!=6)throw std::runtime_error("guard fixture count");
}

void boundary_sound_tests(DualSound*);
static void boundary_actual_tests(ExperimentMachine&machine){
    auto saved=machine.cpu.snapshot();unsigned n=0;
    auto reject=[&](const auto&bad){bool caught=false;try{machine.cpu.restore_component(bad);}catch(const std::runtime_error&){caught=true;}if(!caught||machine.cpu.snapshot()!=saved)throw std::runtime_error("invalid main boundary changed CPU");++n;};
    auto bad=saved;std::get<9>(bad)=1;reject(bad);bad=saved;std::get<10>(bad)=-1;reject(bad);bad=saved;std::get<10>(bad)=4097;reject(bad);
    for(int value:{-1,5}){bad=saved;std::get<11>(bad)=value;reject(bad);}
    bad=saved;std::get<12>(bad)=17;reject(bad);bad=saved;std::get<40>(bad)=1;reject(bad);bad=saved;std::get<41>(bad)=1;reject(bad);
    for(int value:{-1,1}){bad=saved;std::get<42>(bad)=value;reject(bad);}bad=saved;std::get<8>(bad)=0;reject(bad);
    for(auto value:cpu_boundary::scratch_values){auto good=saved;std::get<11>(good)=value;cpu_boundary::validate_main(good);}
    auto mask=saved;std::get<26>(mask)=65535;cpu_boundary::validate_main(mask);
    auto compound=machine.checkpoint();std::get<10>(compound.cpu)=-1;bool caught=false;try{machine.restore(compound);}catch(const std::runtime_error&){caught=true;}compound.cpu=saved;
    if(!caught||!machine_states_equal(compound,machine.checkpoint()))throw std::runtime_error("invalid debt changed compound state");
    if(n!=11)throw std::runtime_error("main boundary test count");boundary_sound_tests(machine.sound.get());
}

static state_file::Bytes pack_machine(const MachineCheckpoint& s){
    state_wire::Writer clock;state_wire::put(clock,s.schema);state_wire::put(clock,s.scenario_identity);state_wire::put(clock,s.host);
    return state_file::envelope({state_wire::encode(s.cpu),state_wire::encode(s.board),sound_checkpoint_pack(s.sound.get()),video_checkpoint_pack(s.video.get()),std::move(clock.bytes)});
}
static MachineCheckpoint unpack_machine(std::span<const uint8_t> bytes){
    auto parts=state_file::unpack(bytes);state_wire::Reader clock{parts[4]};
    unsigned schema;uint64_t profile;std::array<uint64_t,8> host;
    state_wire::get(clock,schema);state_wire::get(clock,profile);state_wire::get(clock,host);clock.finish();
    if(schema!=1)throw std::runtime_error("invalid machine schema");
    return {state_wire::decode<decltype(std::declval<ProbeCpu&>().snapshot())>(parts[0]),state_wire::decode<SunsetBoardSnapshot>(parts[1]),
        SoundState(sound_checkpoint_unpack(parts[2]),sound_checkpoint_delete),VideoState(video_checkpoint_unpack(parts[3]),video_checkpoint_delete),host,profile,schema};
}
int diagnostic_main(int argc,char** argv){try{
    if(argc==2&&std::string(argv[1])=="--self-test"){
        if(main_cpu_component_fixture()!=0)throw std::runtime_error("CPU fixture failed");if(boundary_cpu_fixture()!=0)throw std::runtime_error("cold CPU fixture failed");dual_test();video_checkpoint_test();
        std::cout<<"{\"status\":\"passed\",\"scope\":\"prerequisites\"}\n";return 0;
    }
    if(argc==3&&std::string(argv[1])=="--codec-test"){state_codec_tests(argv[2]);return 0;}
    bool record=argc==6&&std::string(argv[1])=="--record";
    bool resume=argc==7&&std::string(argv[1])=="--resume-fixture";
    if(!record&&!resume)throw std::runtime_error("private fixture mode required");
    auto number=[](std::string s){size_t end=0;auto v=std::stoull(s,&end);if(end!=s.size()||!v||s[0]=='-')throw std::runtime_error("invalid ticks");return v;};
    uint64_t cut=record?number(argv[2]):0,forward=number(argv[record?3:4]);
    std::string prefix=argv[record?4:5],profile=argv[record?5:6];
    state_file::Bytes file;std::optional<MachineCheckpoint> imported;
    if(resume){file=state_file::read(argv[2]);if(state_file::hex(state_file::sha256(file))!=argv[3])throw std::runtime_error("fixture provenance digest mismatch");imported.emplace(unpack_machine(file));cut=imported->host[0];}
    if(cut>58ull*48000000||forward>10ull*48000000||cut+forward>60ull*48000000)throw std::runtime_error("experiment exceeds limits");
    auto events=read_scenario(profile,60ull*48000000/SunsetBus::frame_ticks);
    _setmode(_fileno(stdin),_O_BINARY);
    auto take=[](size_t n){std::vector<uint8_t>b(n);std::cin.read(reinterpret_cast<char*>(b.data()),n);if(std::cin.gcount()!=std::streamsize(n))throw std::runtime_error("truncated regions");return b;};
    auto p=take(0xc0000),e=take(128),c=take(0x200000),m=take(0x10000),v=take(0x100000),t=take(0x100000);
    if(std::cin.peek()!=EOF)throw std::runtime_error("unexpected region bytes");
    ExperimentMachine machine(std::move(p),std::move(e),std::move(c),std::move(m),std::move(v),std::move(t),std::move(events));
    if(record){
        machine.run_until(cut);compact_invalid_test(machine.sound.get());guarded_actual_test(machine);ranged_sound_tests(machine.sound.get());fm_ranged_sound_tests(machine.sound.get());boundary_actual_tests(machine);compact_clear_recording(machine.sound.get());auto saved=machine.checkpoint();file=pack_machine(saved);state_file::write_atomic(prefix+".ssrstate",file);
        imported.emplace(unpack_machine(state_file::read(prefix+".ssrstate")));
        if(!machine_states_equal(saved,*imported))throw std::runtime_error("file roundtrip diverged");
        machine.run_until(cut+forward);auto expected=machine.checkpoint();auto expected_bytes=pack_machine(expected);machine.dump(prefix+".expected");
        machine.restore(*imported);if(!machine_states_equal(*imported,machine.checkpoint()))throw std::runtime_error("file restore diverged");
        machine.run_until(cut+forward);auto actual=machine.checkpoint();machine.dump(prefix+".actual");
        if(!machine_states_equal(expected,actual)||pack_machine(actual)!=expected_bytes)throw std::runtime_error("file continuation diverged");
    }else{machine.restore(*imported);if(!machine_states_equal(*imported,machine.checkpoint()))throw std::runtime_error("cold restore diverged");compact_invalid_test(machine.sound.get());guarded_actual_test(machine);ranged_sound_tests(machine.sound.get());fm_ranged_sound_tests(machine.sound.get());boundary_actual_tests(machine);machine.run_until(cut+forward);machine.dump(prefix);}
    auto final=pack_machine(machine.checkpoint());
    std::cout<<"{\"status\":\"passed\",\"scope\":\"private_own_fixture_only\",\"state_bytes\":"<<file.size()<<",\"state_sha256\":\""<<state_file::hex(state_file::sha256(file))<<"\",\"final_state_sha256\":\""<<state_file::hex(state_file::sha256(final))<<"\",";machine.report();std::cout<<"}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

#include "sunset_menu_adapter.h"
#include "sunset_bar_model.h"
#include "sunset_session_profile.h"
static_assert(SunsetBus::frame_ticks==sunset_session::frame_ticks);
#include "sunset_menu_input.h"
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <chrono>
const uint8_t* window_video_rgb(SunsetVideo*);
void window_audio_sink(DualSound*,std::function<void(int16_t,int16_t)>);
static void sdl_require(bool ok,const char* operation){if(!ok)throw std::runtime_error(std::string(operation)+": "+SDL_GetError());}
struct WindowAudio {
    SDL_AudioStream* stream=nullptr;std::vector<int16_t> pending,trace;
    bool collect=false;uint64_t emitted=0,queue_resets=0;float volume=.5f;
    WindowAudio(){SDL_AudioSpec spec{SDL_AUDIO_S16,2,48000};stream=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
        if(stream){SDL_SetAudioStreamGain(stream,volume);SDL_ResumeAudioStreamDevice(stream);}pending.reserve(1024);}
    ~WindowAudio(){if(stream)SDL_DestroyAudioStream(stream);}
    void push(int16_t l,int16_t r){++emitted;if(collect){trace.push_back(l);trace.push_back(r);}if(!stream)return;
        pending.push_back(l);pending.push_back(r);if(pending.size()>=1024)flush();}
    void flush(){if(stream&&!pending.empty()){
        if(SDL_GetAudioStreamQueued(stream)>38400){SDL_ClearAudioStream(stream);++queue_resets;}
        sdl_require(SDL_PutAudioStreamData(stream,pending.data(),int(pending.size()*2)),"audio queue");pending.clear();}}
    void clear(){pending.clear();trace.clear();if(stream)SDL_ClearAudioStream(stream);}
    void gain(float delta){volume=std::clamp(volume+delta,0.0f,1.0f);if(stream)SDL_SetAudioStreamGain(stream,volume);}
};
struct WindowControls {
    sunset_menu::Turbo* turbo=nullptr;sunset_menu::Policy* policy=nullptr;sunset_menu::ReleaseGate release_gate;
    SDL_Window* window=nullptr;std::array<SDL_Gamepad*,4> pads{};std::array<bool,4> coin_was{},start_was{};std::array<unsigned,4> pulse{};bool service_blocked=false;bool service_was=false;unsigned service_pulse=0;
    ~WindowControls(){for(auto*p:pads)if(p)SDL_CloseGamepad(p);}
    void connect(SDL_JoystickID id){for(auto*p:pads)if(p&&SDL_GetGamepadID(p)==id)return;for(auto&p:pads)if(!p){p=SDL_OpenGamepad(id);release_gate.block(unsigned(&p-&pads[0]));return;}}
    void disconnect(SDL_JoystickID id){for(unsigned i=0;i<4;++i)if(pads[i]&&SDL_GetGamepadID(pads[i])==id){SDL_CloseGamepad(pads[i]);pads[i]=nullptr;start_was[i]=false;pulse[i]=0;release_gate.block(i);}}
    void clear(){pulse.fill(0);service_pulse=0;service_blocked=true;release_gate.block_all();}
    void latch(SunsetBus&board){
        SDL_PumpEvents();SDL_UpdateGamepads();const bool*keys=SDL_GetKeyboardState(nullptr);bool focus=(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS)!=0;
        static constexpr SDL_Scancode map[4][6]={{SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN,SDL_SCANCODE_Z,SDL_SCANCODE_X},
            {SDL_SCANCODE_A,SDL_SCANCODE_D,SDL_SCANCODE_W,SDL_SCANCODE_S,SDL_SCANCODE_F,SDL_SCANCODE_G},
            {SDL_SCANCODE_J,SDL_SCANCODE_L,SDL_SCANCODE_I,SDL_SCANCODE_K,SDL_SCANCODE_U,SDL_SCANCODE_O},
            {SDL_SCANCODE_KP_4,SDL_SCANCODE_KP_6,SDL_SCANCODE_KP_8,SDL_SCANCODE_KP_5,SDL_SCANCODE_KP_1,SDL_SCANCODE_KP_3}};
        static constexpr SDL_Scancode coins[4]={SDL_SCANCODE_1,SDL_SCANCODE_2,SDL_SCANCODE_3,SDL_SCANCODE_4};std::array<uint8_t,4> held{};uint8_t c=0;
        for(unsigned p=0;p<4;++p){bool coin=focus&&keys[coins[p]],start=false;
            if(focus)for(unsigned b=0;b<6;++b)if(keys[map[p][b]])held[p]|=1u<<b;
            if(focus&&pads[p]){auto*g=pads[p];int x=SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_LEFTX),y=SDL_GetGamepadAxis(g,SDL_GAMEPAD_AXIS_LEFTY);
                if(x<-16000||SDL_GetGamepadButton(g,SDL_GAMEPAD_BUTTON_DPAD_LEFT))held[p]|=1;
                if(x>16000||SDL_GetGamepadButton(g,SDL_GAMEPAD_BUTTON_DPAD_RIGHT))held[p]|=2;
                if(y<-16000||SDL_GetGamepadButton(g,SDL_GAMEPAD_BUTTON_DPAD_UP))held[p]|=4;
                if(y>16000||SDL_GetGamepadButton(g,SDL_GAMEPAD_BUTTON_DPAD_DOWN))held[p]|=8;
                if(SDL_GetGamepadButton(g,SDL_GAMEPAD_BUTTON_SOUTH))held[p]|=16;
                if(SDL_GetGamepadButton(g,SDL_GAMEPAD_BUTTON_EAST))held[p]|=32;
                start=SDL_GetGamepadButton(g,SDL_GAMEPAD_BUTTON_START);
            }
            if(!release_gate.accept(p,held[p]!=0||coin||start)){held[p]=0;coin=false;start=false;pulse[p]=0;}
            if(turbo&&policy)held[p]=turbo->apply(p,held[p],*policy);
            if((coin&&!coin_was[p])||(start&&!start_was[p]))pulse[p]=5;coin_was[p]=coin;start_was[p]=start;
            if(pulse[p]&&focus){c|=1u<<p;--pulse[p];}
        }
        bool service=focus&&keys[SDL_SCANCODE_F2];if(service_blocked){if(!service)service_blocked=false;service=false;}if(service&&!service_was)service_pulse=5;service_was=service;uint8_t s=0;if(service_pulse&&focus){s=1;--service_pulse;}
        if(!focus)clear();board.inputs.frame(held,c,s);
    }
};
static void resource_test(){
    auto r=FindResourceW(nullptr,MAKEINTRESOURCEW(1),MAKEINTRESOURCEW(14));if(!r)throw std::runtime_error("embedded game icon missing");
    auto h=LoadResource(nullptr,r);auto*p=static_cast<const uint8_t*>(LockResource(h));auto size=SizeofResource(nullptr,r);
    if(size<6||p[4]!=7||p[5]!=0)throw std::runtime_error("embedded icon sizes missing");
    std::set<unsigned> sizes;for(unsigned i=0;i<7;++i){size_t at=6+14*i;if(at+14>size)throw std::runtime_error("truncated icon group");sizes.insert(p[at]?p[at]:256);}
    if(sizes!=std::set<unsigned>{16,24,32,48,64,128,256})throw std::runtime_error("incorrect embedded game icon sizes");
    std::cout<<"{\"status\":\"passed\",\"embedded_game_icon_sizes\":7}\n";
}
static std::unique_ptr<ExperimentMachine> load_window_machine(const std::string&scenario){
    _setmode(_fileno(stdin),_O_BINARY);auto take=[](size_t n){std::vector<uint8_t>b(n);std::cin.read(reinterpret_cast<char*>(b.data()),n);if(std::cin.gcount()!=std::streamsize(n))throw std::runtime_error("truncated verified ROM pipe");return b;};
    auto p=take(0xc0000),e=take(128),c=take(0x200000),m=take(0x10000),v=take(0x100000),t=take(0x100000);if(std::cin.peek()!=EOF)throw std::runtime_error("extra ROM pipe data");
    auto events=scenario.empty()?std::vector<SunsetInputEvent>{}:read_scenario(scenario,sunset_session::max_frames);
    return std::make_unique<ExperimentMachine>(std::move(p),std::move(e),std::move(c),std::move(m),std::move(v),std::move(t),std::move(events));
}
// Invoked only by explicit --live-input-test; no arbitrary memory writes.
static void live_input_qa(ExperimentMachine& machine,WindowControls& controls,sunset_menu::Adapter& overlay){
    auto demand=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    for(auto*pad:controls.pads)if(pad)controls.disconnect(SDL_GetGamepadID(pad));
    std::array<SDL_Gamepad*,4> virtual_pads{};std::array<SDL_JoystickID,4> ids{};
    struct Cleanup {std::array<SDL_JoystickID,4>&ids;~Cleanup(){for(auto id:ids)if(id)SDL_DetachVirtualJoystick(id);}} cleanup{ids};
    for(unsigned p=0;p<4;++p){SDL_VirtualJoystickDesc d{};SDL_INIT_INTERFACE(&d);d.type=SDL_JOYSTICK_TYPE_GAMEPAD;d.naxes=6;d.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;d.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;d.axis_mask=63;d.name="Sunset live-latch QA";
        ids[p]=SDL_AttachVirtualJoystick(&d);demand(ids[p]!=0,"virtual attach failed");controls.connect(ids[p]);virtual_pads[p]=controls.pads[p];demand(virtual_pads[p]&&SDL_GetGamepadID(virtual_pads[p])==ids[p],"virtual slot ownership failed");
        demand(overlay.turbo.set(p,p*5,overlay.policy),"turbo setup denied");
    }
    SDL_RaiseWindow(controls.window);SDL_Delay(100);SDL_PumpEvents();
    demand((SDL_GetWindowFlags(controls.window)&SDL_WINDOW_INPUT_FOCUS)!=0,"live QA window must have keyboard focus");
    std::array<unsigned,4> rises{},on_frames{},gated{},later{};std::array<bool,4> before{};unsigned latch=0,slot_checks=0;
    auto button=[&](unsigned p,SDL_GamepadButton b,bool value){demand(SDL_SetJoystickVirtualButton(SDL_GetGamepadJoystick(virtual_pads[p]),b,value),"virtual button failed");};
    machine.live_input=[&](uint64_t){
        bool active=(latch>=450&&latch<1050)||(latch>=1100&&latch<1160);
        for(unsigned p=0;p<4;++p){button(p,SDL_GAMEPAD_BUTTON_START,latch==300);button(p,SDL_GAMEPAD_BUTTON_SOUTH,active);button(p,SDL_GAMEPAD_BUTTON_EAST,active);}
        // Same live latch and real board input ports as manual play, not scenario injection.
        controls.latch(machine.board);
        for(unsigned p=0;p<4;++p){auto value=machine.board.inputs.held[p];bool fire=(value&16)!=0;
            if(latch>=450&&latch<750){demand((value&32)!=0,"turbo changed jump input");on_frames[p]+=fire;if(fire&&!before[p])++rises[p];}
            if(latch>=750&&latch<810){demand(value==0,"held input escaped menu release barrier");++gated[p];}
            if(latch>=1100&&latch<1160){demand((value&48)==48,"fresh physical edge failed after neutral");++later[p];}
            before[p]=fire;
        }
        ++latch;
    };
    for(unsigned f=0;f<1200;++f){
        if(latch==750)controls.clear();
        if(latch==900){
            auto guest=machine.checkpoint();auto host=overlay.turbo.snapshot();
            for(unsigned p=0;p<4;++p)overlay.turbo.set(p,0,overlay.policy);
            machine.restore(guest);overlay.turbo.restore(host);controls.clear();
            demand(overlay.turbo.snapshot().phase==host.phase&&overlay.turbo.snapshot().rate==host.rate,"slot host phase restore mismatch");++slot_checks;
        }
        if(latch==1100)for(unsigned p=0;p<4;++p)overlay.turbo.set(p,0,overlay.policy);
        machine.run_until((machine.elapsed/SunsetBus::frame_ticks+1)*SunsetBus::frame_ticks);
        if(latch>=1200)break;
    }
    demand(latch>=1160&&slot_checks==1,"live QA incomplete");
    for(unsigned p=0;p<4;++p){demand(gated[p]==60,"per-player release gate missing");if(p){unsigned expected=unsigned(300ull*SunsetBus::frame_ticks*p*5/48000000);demand(rises[p]>=expected&&rises[p]<=expected+1,"live turbo pulse rate mismatch");demand(on_frames[p]>0&&on_frames[p]<300,"turbo release gaps missing");}else demand(on_frames[p]==300,"Off changed held fire");}
    demand(overlay.policy.tainted(),"live turbo did not taint session");
    std::cout<<"{\"status\":\"passed\",\"virtual_gamepads\":4,\"live_frame_latches\":"<<latch<<",\"slot_metadata_checks\":"<<slot_checks<<",\"pulses\":["<<rises[0]<<','<<rises[1]<<','<<rises[2]<<','<<rises[3]<<"],\"physical_acceptance\":false}\n";
}

static void assist_integration(ExperimentMachine&m){
    window_audio_sink(m.sound.get(),[](int16_t,int16_t){});m.run_until(1500ull*SunsetBus::frame_ticks);auto saved=m.checkpoint();
    auto rb=[&](unsigned addr){auto w=m.board.read(addr&~1u,0xffff,false);return uint8_t((addr&1)?w:w>>8);};
    unsigned cases=0;for(unsigned p=0;p<4;++p)for(unsigned mask:{1u,2u,3u}){
        m.before_frame={};m.restore(saved);window_audio_sink(m.sound.get(),[](int16_t,int16_t){});
        sunset_menu::Assists a;sunset_menu::Policy policy;if(!a.bind(state_file::sha256(m.board.rom))||!a.request(p,mask,policy))throw std::runtime_error("assist profile/request failed");
        m.before_frame=[&]{a.tick(m.board,policy);};m.run_until(m.elapsed+12ull*SunsetBus::frame_ticks);
        for(unsigned q=0;q<4;++q)if(rb(0x181c80+q*0x300+0x27)!=(p==q?mask:0)||m.board.ram[0x211+q*0x40]!=(p==q?mask:0))throw std::runtime_error("native pickup/player isolation mismatch");
        if(rb(0x181c80+p*0x300+0x1b)!=((mask&2)?8:4)||!policy.tainted())throw std::runtime_error("native animation/policy mismatch");
        auto count=a.pickups;if(!a.request(p,mask,policy))throw std::runtime_error("duplicate request failed");m.run_until(m.elapsed+2ull*SunsetBus::frame_ticks);if(a.pickups!=count)throw std::runtime_error("duplicate powerup injected");
        ++cases;
    }
    m.before_frame={};m.restore(saved);window_audio_sink(m.sound.get(),[](int16_t,int16_t){});
    sunset_menu::Assists a;sunset_menu::Policy policy;a.bind(state_file::sha256(m.board.rom));a.set_lives(0,true,policy);
    m.before_frame=[&]{a.tick(m.board,policy);};m.live_input=[&](uint64_t){m.board.inputs.frame({18,18,18,18},0,0);};
    m.run_until(m.elapsed+2000ull*SunsetBus::frame_ticks);
    // P1 staying alive can change native enemy/camera behaviour for P2; compare assist writes, not a prescribed P2 death outcome.
    if(m.board.ram[0x200]<2||!a.topups||!policy.tainted()||a.topups_by_player[1]||a.topups_by_player[2]||a.topups_by_player[3])throw std::runtime_error("native life isolation mismatch: P1="+std::to_string(m.board.ram[0x200])+" P2="+std::to_string(m.board.ram[0x240])+" topups="+std::to_string(a.topups));
    auto host=a.snapshot();a.set_lives(0,false,policy);a.restore(host);if(!a.infinite(0))throw std::runtime_error("own-slot assist restore failed");
    std::cout<<"{\"status\":\"passed\",\"powerup_cases\":"<<cases<<",\"p1_topups\":"<<a.topups<<",\"p1_lives\":"<<unsigned(m.board.ram[0x200])<<",\"unassisted_p2_lives\":"<<unsigned(m.board.ram[0x240])<<",\"physical_acceptance\":false}\n";
    m.before_frame={};m.live_input={};
}

#include <psapi.h>
struct SoakMemory {uint64_t private_bytes,working_bytes;};
static SoakMemory soak_memory(){PROCESS_MEMORY_COUNTERS_EX c{};c.cb=sizeof(c);if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&c),sizeof(c)))throw std::runtime_error("own-process memory query failed");return {uint64_t(c.PrivateUsage),uint64_t(c.WorkingSetSize)};}
static void long_soak(ExperimentMachine&m,const std::string&capture){
    constexpr uint64_t hz=48000000,end=480ull*hz;auto wall=std::chrono::steady_clock::now();
    struct AudioDigest {uint64_t hash=14695981039346656037ull,count=0;bool collect=false;void add(int16_t l,int16_t r){if(!collect)return;for(auto v:{uint16_t(l),uint16_t(r)})for(unsigned shift:{0u,8u})hash=(hash^uint8_t(v>>shift))*1099511628211ull;++count;}} audio;
    auto bind=[&]{window_audio_sink(m.sound.get(),[&](int16_t l,int16_t r){audio.add(l,r);});};bind();
    m.run_until(1500ull*SunsetBus::frame_ticks);
    std::array<unsigned,4> previous{},deaths{},rejoins{},coin_requests{};for(unsigned p=0;p<4;++p)previous[p]=m.board.ram[0x200+p*0x40];
    bool observe=true;std::set<unsigned> stages;std::vector<SoakMemory> samples;unsigned minute=1,restore_checks=0;uint64_t saved_bytes=0;
    m.live_input=[&](uint64_t frame){uint8_t coins=0;
        for(unsigned p=0;p<4;++p){unsigned lives=m.board.ram[0x200+p*0x40];if(observe){if(lives<previous[p])++deaths[p];if(previous[p]==0&&lives>0)++rejoins[p];previous[p]=lives;}
            if(frame%600<2&&lives==0&&!(m.board.ram[0x170]&(1u<<p))){coins|=1u<<p;if(observe&&frame%600==0)++coin_requests[p];}}
        std::array<uint8_t,4> held{};for(unsigned p=0;p<4;++p)held[p]=uint8_t(16|(frame%600<480?2:0)|(frame%120<8?32:0));m.board.inputs.frame(held,coins,0);
    };
    while(m.elapsed<end){
        m.run_until((m.elapsed/SunsetBus::frame_ticks+1)*SunsetBus::frame_ticks);stages.insert(m.board.ram[0x300]);
        if(m.elapsed>=minute*60ull*hz){auto mem=soak_memory();samples.push_back(mem);
            if(!capture.empty())video_write_ppm(m.video.get(),capture+"-minute-"+std::to_string(minute)+".ppm");
            std::cout<<"{\"progress_minute\":"<<minute<<",\"private_bytes\":"<<mem.private_bytes<<",\"working_bytes\":"<<mem.working_bytes<<",\"stage\":"<<unsigned(m.board.ram[0x300])<<"}\n"<<std::flush;
            if(minute==6){
                auto saved=m.checkpoint();saved_bytes=pack_machine(saved).size();audio={};audio.collect=true;
                m.run_until(m.elapsed+90ull*SunsetBus::frame_ticks);auto expected=m.checkpoint();auto expected_bytes=pack_machine(expected);auto expected_audio=audio;
                m.restore(saved);bind();audio={};audio.collect=true;observe=false;m.run_until(saved.host[0]+90ull*SunsetBus::frame_ticks);
                if(pack_machine(m.checkpoint())!=expected_bytes||audio.hash!=expected_audio.hash||audio.count!=expected_audio.count)throw std::runtime_error("post-six-minute state/audio continuation mismatch");
                audio.collect=false;observe=true;for(unsigned p=0;p<4;++p)previous[p]=m.board.ram[0x200+p*0x40];++restore_checks;
            }
            ++minute;
        }
    }
    if(samples.size()!=8||restore_checks!=1)throw std::runtime_error("long session observation incomplete");
    uint64_t peak=0;for(const auto&s:samples)peak=std::max(peak,s.private_bytes);uint64_t growth=peak>samples.front().private_bytes?peak-samples.front().private_bytes:0;
    if(growth>64ull*1024*1024)throw std::runtime_error("private memory growth exceeds soak threshold");
    std::cout<<"{\"status\":\"passed\",\"simulated_seconds\":480,\"restore_checks\":"<<restore_checks<<",\"post_warmup_peak_growth_bytes\":"<<growth<<",\"checkpoint_bytes\":"<<saved_bytes<<",\"life_decrements\":[";
    for(unsigned p=0;p<4;++p){if(p)std::cout<<',';std::cout<<deaths[p];}std::cout<<"],\"native_rejoins\":[";
    for(unsigned p=0;p<4;++p){if(p)std::cout<<',';std::cout<<rejoins[p];}std::cout<<"],\"stages\":[";bool comma=false;for(auto stage:stages){if(comma)std::cout<<',';comma=true;std::cout<<stage;}
    std::cout<<"],\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-wall).count()<<",\"headless\":true,\"full_game_validated\":false}\n";
    m.live_input={};
}

int main(int argc,char**argv){try{
    if(argc==2&&std::string(argv[1])=="--resource-test"){resource_test();return 0;}
    if(argc<2||std::string(argv[1])!="--window")return diagnostic_main(argc,argv);
    bool unpaced=false,qa=false,menu_qa=false,live_test=false,assist_test=false,bar_qa=false,soak_test=false;uint64_t stop=sunset_session::max_frames;std::string scenario,capture;
    for(int i=2;i<argc;++i){std::string a=argv[i];if(a=="--long-session-test")soak_test=true;else if(a=="--bar-ui-test")bar_qa=true;else if(a=="--assist-test")assist_test=true;else if(a=="--live-input-test")live_test=true;else if(a=="--menu-ui-test")menu_qa=true;else if(a=="--unpaced")unpaced=true;else if(a=="--ui-test")qa=true;
        else if((a=="--frames"||a=="--scenario"||a=="--capture")&&i+1<argc){std::string value=argv[++i];if(a=="--frames")stop=std::stoull(value);if(a=="--scenario")scenario=value;if(a=="--capture")capture=value;}
        else throw std::runtime_error("unknown window option");}
    if(!stop||stop>sunset_session::max_frames||(qa&&(scenario.empty()||stop<1500||stop>sunset_session::qa_frames)))throw std::runtime_error("invalid laboratory window bounds");
    auto machine=load_window_machine(scenario);if(soak_test){long_soak(*machine,capture);return 0;}if(assist_test){assist_integration(*machine);return 0;}SDL_SetMainReady();sdl_require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMEPAD),"SDL init");
    struct SdlLifetime {~SdlLifetime(){SDL_Quit();}} sdl_lifetime;
    auto*w=SDL_CreateWindow("Sunset Riders EAC 4P | ForkAde laboratorio | sesion extendida",864,804,SDL_WINDOW_RESIZABLE);sdl_require(w!=nullptr,"window");
    auto*r=SDL_CreateRenderer(w,nullptr);sdl_require(r!=nullptr,"renderer");SDL_SetRenderVSync(r,0);SDL_SetRenderLogicalPresentation(r,576,536,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    auto*texture=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,288,224);sdl_require(texture!=nullptr,"framebuffer texture");SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_NEAREST);
    std::string icon=std::string(SDL_GetBasePath())+"title-icon.bmp";if(auto*s=SDL_LoadBMP(icon.c_str())){SDL_SetWindowIcon(w,s);SDL_DestroySurface(s);}
    sunset_menu::PadMenuInput pad_menu;sunset_menu::Adapter overlay;overlay.assists.bind(state_file::sha256(machine->board.rom));machine->before_frame=[&]{overlay.assists.tick(machine->board,overlay.policy);}; unsigned menu_opened=0; bool auto_menu=false; state_file::Bytes menu_before;
    WindowAudio audio;audio.collect=qa;WindowControls controls;controls.window=w;controls.turbo=&overlay.turbo;controls.policy=&overlay.policy;int count=0;auto*ids=SDL_GetGamepads(&count);for(int i=0;i<count;++i)controls.connect(ids[i]);SDL_free(ids);if(live_test){live_input_qa(*machine,controls,overlay);return 0;}
    if(scenario.empty())machine->live_input=[&](uint64_t){controls.latch(machine->board);};
    auto bind_audio=[&](){window_audio_sink(machine->sound.get(),[&](int16_t l,int16_t rr){audio.push(l,rr);});};bind_audio();
    std::optional<sunset_menu::Assists::Snapshot> assist_slot;std::optional<sunset_menu::Turbo::Snapshot> turbo_slot;std::optional<MachineCheckpoint> slot,expected;state_file::Bytes expected_bytes;std::vector<int16_t> expected_audio;unsigned stage=0,saves=0,loads=0;bool quit=false,paused=false,fullscreen=false;
    uint64_t anchor_wall=SDL_GetTicksNS(),anchor_master=0,presented=0;std::string status="Laboratorio local. Slot interno: se pierde al salir.";
    auto reanchor=[&](){anchor_wall=SDL_GetTicksNS();anchor_master=machine->elapsed;};
    auto key=[&](SDL_Scancode code){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.scancode=code;e.key.down=true;SDL_PushEvent(&e);};
    recomp::MenuBar bar;sunset_menu::BarModel bar_model(overlay);bar.SetArea(576,536,2);
    overlay.open();overlay.menu.Close();
    auto refresh_bar=[&]{bar.Configure(bar_model.entries(bool(slot),machine->elapsed>=SunsetBus::frame_ticks));};refresh_bar();
    bool bar_prior_pause=false;unsigned bar_stage=0,coordinate_checks=0;state_file::Bytes bar_before;
    auto menu_button=[](SDL_Scancode c){return c==SDL_SCANCODE_UP?recomp::kMenuUp:c==SDL_SCANCODE_DOWN?recomp::kMenuDown:c==SDL_SCANCODE_LEFT?recomp::kMenuLeft:c==SDL_SCANCODE_RIGHT?recomp::kMenuRight:c==SDL_SCANCODE_RETURN||c==SDL_SCANCODE_KP_ENTER?recomp::kMenuConfirm:c==SDL_SCANCODE_BACKSPACE||c==SDL_SCANCODE_ESCAPE?recomp::kMenuBack:0;};
    auto hush=[&]{audio.clear();controls.clear();reanchor();};
    auto sync_sound=[&]{audio.volume=overlay.muted?0.0f:overlay.volume/100.0f;if(audio.stream)SDL_SetAudioStreamGain(audio.stream,audio.volume);};
    auto dispatch=[&](recomp::Menu::Chosen action){
        if(action.kind==recomp::Menu::Chosen::Quit)quit=true;
        if(action.kind==recomp::Menu::Chosen::Save&&overlay.policy.allows(sunset_menu::Action::Save)&&machine->elapsed>=SunsetBus::frame_ticks){slot.emplace(machine->checkpoint());turbo_slot=overlay.turbo.snapshot();assist_slot=overlay.assists.snapshot();++saves;audio.trace.clear();status="Slot temporal guardado. F8 para cargar.";overlay.menu.Close();}
        if(action.kind==recomp::Menu::Chosen::Load&&overlay.policy.allows(sunset_menu::Action::Load)){
            if(slot){if(!assist_slot||!turbo_slot)throw std::runtime_error("own-slot metadata missing");machine->restore(*slot);overlay.turbo.restore(*turbo_slot);overlay.assists.restore(*assist_slot);bind_audio();hush();++loads;status="Slot temporal restaurado.";overlay.menu.Close();}
            else {status="No hay slot guardado. Usa F5.";overlay.menu.Note(status);}
        }
        if(action.kind==recomp::Menu::Chosen::Action){
            if(action.id==sunset_menu::Adapter::Fullscreen){fullscreen=!fullscreen;SDL_SetWindowFullscreen(w,fullscreen);}
            if(action.id==sunset_menu::Adapter::Controls){status="P1 arrows Z/X; P2 WASD F/G; P3 IJKL U/O; P4 keypad.";overlay.menu.Note(status);}
            if(action.id==sunset_menu::BarModel::About)status="EAC local: CPU interpretadas; sesion extendida; sin online.";
            if(action.id==sunset_menu::BarModel::Shortcuts)status="Esc menu; F10 barra; F5 guardar; F8 cargar; F11 pantalla.";
        }
        sync_sound();refresh_bar();
    };
    auto opened_bar=[&]{bar_prior_pause=paused&&!overlay.menu.IsOpen();overlay.menu.Close();refresh_bar();if(overlay.policy.local())paused=true;hush();};
    auto update_bar_pause=[&]{if(!bar.IsOpen()){paused=bar_prior_pause||overlay.menu.IsOpen();hush();}};
    auto chose_bar=[&](recomp::MenuBar::Chosen choice){if(choice.chosen){dispatch(bar_model.choose(choice.id));bar.Close();}update_bar_pause();};
    auto click_logical=[&](float x,float y){float wx,wy,lx,ly;sdl_require(SDL_RenderCoordinatesToWindow(r,x,y,&wx,&wy),"logical mouse conversion");sdl_require(SDL_RenderCoordinatesFromWindow(r,wx,wy,&lx,&ly),"window mouse conversion");if(std::abs(lx-x)>.05f||std::abs(ly-y)>.05f)throw std::runtime_error("letterbox coordinates diverged");++coordinate_checks;SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.windowID=SDL_GetWindowID(w);e.button.button=SDL_BUTTON_LEFT;e.button.x=wx;e.button.y=wy;SDL_PushEvent(&e);};
    auto*bar_texture=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,576,536);sdl_require(bar_texture!=nullptr,"bar texture");SDL_SetTextureBlendMode(bar_texture,SDL_BLENDMODE_NONE);

    while(!quit){SDL_Event e;while(SDL_PollEvent(&e)){
        if(e.type==SDL_EVENT_QUIT)quit=true;
        if(e.type==SDL_EVENT_GAMEPAD_ADDED)controls.connect(e.gdevice.which);if(e.type==SDL_EVENT_GAMEPAD_REMOVED){controls.disconnect(e.gdevice.which);pad_menu.remove(e.gdevice.which);}
        if(e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN||e.type==SDL_EVENT_GAMEPAD_AXIS_MOTION){
            SDL_JoystickID id=e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN?e.gbutton.which:e.gaxis.which;bool known=false;
            for(auto*pad:controls.pads)if(pad&&SDL_GetGamepadID(pad)==id)known=true;
            if(known&&!qa){auto translated=pad_menu.translate(e,overlay.menu.IsOpen()||bar.IsOpen());if(translated!=SDL_SCANCODE_UNKNOWN)key(translated);}
        }
        if(e.type==SDL_EVENT_WINDOW_FOCUS_LOST&&!qa){paused=true;bar.Close();overlay.menu.Close();pad_menu.clear();audio.clear();controls.clear();}
        if(e.type==SDL_EVENT_MOUSE_MOTION&&bar.IsOpen()&&!overlay.menu.IsOpen()){float x,y;sdl_require(SDL_RenderCoordinatesFromWindow(r,e.motion.x,e.motion.y,&x,&y),"pointer coordinates");bar.Pointer(x,y);}
        if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&!overlay.menu.IsOpen()){
            float x,y;sdl_require(SDL_RenderCoordinatesFromWindow(r,e.button.x,e.button.y,&x,&y),"click coordinates");
            if(e.button.button==SDL_BUTTON_LEFT&&(bar.IsOpen()||bar.Hit(x,y))){if(!bar.IsOpen())opened_bar();chose_bar(bar.Click(x,y));}
            else if(e.button.button==SDL_BUTTON_RIGHT){if(!bar.IsOpen())opened_bar();bar.OpenPopup(x,y);}
        }
        if(e.type==SDL_EVENT_KEY_DOWN&&!e.key.repeat){auto c=e.key.scancode;
            if(c==SDL_SCANCODE_F10){if(bar.IsOpen()){bar.Close();update_bar_pause();}else{opened_bar();bar.OpenMenu(0);}continue;}
            if(bar.IsOpen()){unsigned b=menu_button(c);if(b)chose_bar(bar.Buttons(b));continue;}

            if(c==SDL_SCANCODE_ESCAPE){
                if(overlay.menu.IsOpen())overlay.menu.Close();else {overlay.open();++menu_opened;pad_menu.clear();if(menu_qa)menu_before=pack_machine(machine->checkpoint());}
                paused=overlay.menu.IsOpen();audio.clear();controls.clear();reanchor();continue;
            }
            if(overlay.menu.IsOpen()) {
                unsigned b=menu_button(c);if(b){dispatch(overlay.press(b));paused=overlay.menu.IsOpen();hush();continue;}
            }
            if(c==SDL_SCANCODE_P&&!overlay.menu.IsOpen()){paused=!paused;audio.clear();controls.clear();reanchor();}
            if(c==SDL_SCANCODE_Q&&paused)quit=true;
            if(c==SDL_SCANCODE_EQUALS||c==SDL_SCANCODE_KP_PLUS||c==SDL_SCANCODE_MINUS||c==SDL_SCANCODE_KP_MINUS){
                bool up=c==SDL_SCANCODE_EQUALS||c==SDL_SCANCODE_KP_PLUS;overlay.volume=std::clamp(overlay.volume+(up?5:-5),0,100);overlay.muted=false;audio.volume=overlay.volume/100.0f;if(audio.stream)SDL_SetAudioStreamGain(audio.stream,audio.volume);
            }
            if(c==SDL_SCANCODE_F11){fullscreen=!fullscreen;SDL_SetWindowFullscreen(w,fullscreen);}
            if(c==SDL_SCANCODE_F5)dispatch({recomp::Menu::Chosen::Save,0});
            if(c==SDL_SCANCODE_F8)dispatch({recomp::Menu::Chosen::Load,0});
        }
    }
    if(quit)break;
    if(bar_qa&&bar_stage==0&&machine->drawn>=700){SDL_SetWindowSize(w,1000,700);SDL_SyncWindow(w);auto box=bar.TitleBox(0);click_logical(box.x+box.w/2,box.y+box.h/2);bar_stage=1;}
    if(bar_qa&&bar_stage==1&&bar.IsOpen()){bar_before=pack_machine(machine->checkpoint());bar_stage=2;}
    if(bar_qa&&bar_stage==3&&!bar.IsOpen()){if(pack_machine(machine->checkpoint())!=bar_before)throw std::runtime_error("mouse menu changed guest state");bar_stage=4;}

    if(menu_qa&&!auto_menu&&machine->drawn>=500){menu_before=pack_machine(machine->checkpoint());key(SDL_SCANCODE_ESCAPE);auto_menu=true;}
    if(menu_qa&&overlay.menu.IsOpen()){
        if(pack_machine(machine->checkpoint())!=menu_before)throw std::runtime_error("menu mutated canonical state");
        key(SDL_SCANCODE_ESCAPE);
    }
    if(!paused){uint64_t target=(machine->elapsed/SunsetBus::frame_ticks+1)*SunsetBus::frame_ticks;machine->run_until(target);audio.flush();
        if(qa&&stage==0&&machine->drawn>=1050){key(SDL_SCANCODE_F5);stage=1;}
        else if(qa&&stage==1&&machine->drawn>=1140){expected.emplace(machine->checkpoint());expected_bytes=pack_machine(*expected);expected_audio=audio.trace;key(SDL_SCANCODE_F8);stage=2;}
        else if(qa&&stage==2&&machine->drawn>=1140){auto actual=machine->checkpoint();if(pack_machine(actual)!=expected_bytes||audio.trace!=expected_audio)throw std::runtime_error("SDL slot continuation state/audio diverged");stage=3;status="UI TEST: estado y audio exactos tras F5/F8.";}
        if(machine->drawn>=stop)quit=true;
    }
    std::vector<uint8_t> menu_rgb;const uint8_t* rgb=window_video_rgb(machine->video.get());
    if(overlay.menu.IsOpen()){
      std::vector<uint32_t> pixels(288*224);for(size_t i=0;i<pixels.size();++i)pixels[i]=rgb[i*3]|(uint32_t(rgb[i*3+1])<<8)|(uint32_t(rgb[i*3+2])<<16)|0xff000000;
      overlay.menu.Draw(pixels.data(),288,224);menu_rgb.resize(288*224*3);
      for(size_t i=0;i<pixels.size();++i){menu_rgb[i*3]=pixels[i];menu_rgb[i*3+1]=pixels[i]>>8;menu_rgb[i*3+2]=pixels[i]>>16;}rgb=menu_rgb.data();
    }
    sdl_require(SDL_UpdateTexture(texture,nullptr,rgb,288*3),"texture update");SDL_SetRenderDrawColor(r,12,12,16,255);SDL_RenderClear(r);SDL_FRect game{0,24,576,448};SDL_RenderTexture(r,texture,nullptr,&game);
    SDL_SetRenderDrawColor(r,230,230,235,255);SDL_RenderDebugText(r,8,475,"P1 Flechas Z/X 1 | P2 WASD F/G 2 | P3 IJKL U/O 3");SDL_RenderDebugText(r,8,487,"P4 KP4/6/8/5 KP1/3 4 | Mandos: A/B y Start=moneda");
    std::string info="F5 slot | F8 cargar | Esc menu | P pausa | +/- vol "+std::to_string(int(audio.volume*100))+"% | F11";SDL_RenderDebugText(r,8,499,info.c_str());SDL_RenderDebugText(r,8,511,status.c_str());SDL_RenderDebugText(r,8,523,"EAC 4P | Perfil local hasta 12 h (experimental) | Sin online");
    if(paused&&!overlay.menu.IsOpen()&&!bar.IsOpen()){SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(r,0,0,0,225);SDL_FRect panel{80,130,416,154};SDL_RenderFillRect(r,&panel);SDL_SetRenderDrawColor(r,255,220,80,255);SDL_RenderDebugText(r,112,160,"PAUSA - Sunset Riders laboratorio");SDL_RenderDebugText(r,112,185,"Esc/P: seguir   Q: salir");SDL_RenderDebugText(r,112,210,"F5: guardar slot   F8: restaurar");SDL_RenderDebugText(r,112,235,"+/-: volumen   F11: pantalla completa");}
    std::vector<uint32_t> bar_pixels(576*536,0xff000000);bar.Draw(bar_pixels.data(),576,536);
    sdl_require(SDL_UpdateTexture(bar_texture,nullptr,bar_pixels.data(),576*4),"bar pixels");
    SDL_FRect strip{0,0,576,float(bar.Height())};SDL_RenderTexture(r,bar_texture,&strip,&strip);
    for(auto box:bar.Boxes()){SDL_FRect rect{box.x,box.y,box.w,box.h};SDL_RenderTexture(r,bar_texture,&rect,&rect);}
    if(bar_qa&&bar_stage==2){if(!capture.empty()){auto*shot=SDL_RenderReadPixels(r,nullptr);sdl_require(shot!=nullptr,"bar capture");sdl_require(SDL_SaveBMP(shot,(capture+"-bar.bmp").c_str()),"bar screenshot");SDL_DestroySurface(shot);}click_logical(570,530);bar_stage=3;}
    if(quit&&!capture.empty()){auto*s=SDL_RenderReadPixels(r,nullptr);sdl_require(s!=nullptr,"window capture");sdl_require(SDL_SaveBMP(s,(capture+".bmp").c_str()),"capture BMP");SDL_DestroySurface(s);video_write_ppm(machine->video.get(),capture+".ppm");}
    if(menu_qa&&overlay.menu.IsOpen()&&!capture.empty()){auto*shot=SDL_RenderReadPixels(r,nullptr);sdl_require(shot!=nullptr,"menu capture");sdl_require(SDL_SaveBMP(shot,(capture+"-menu.bmp").c_str()),"menu image");SDL_DestroySurface(shot);}
    SDL_RenderPresent(r);++presented;
    if(paused)SDL_Delay(16);else if(!unpaced){auto ticks=machine->elapsed-anchor_master;uint64_t due=anchor_wall+(ticks/48000000)*1000000000+(ticks%48000000)*1000000000/48000000;auto now=SDL_GetTicksNS();if(due>now)SDL_DelayNS(due-now);}
    }
    if(bar_qa&&(bar_stage!=4||coordinate_checks!=2))throw std::runtime_error("mouse bar QA incomplete");
    if(menu_qa&&menu_opened!=1)throw std::runtime_error("menu UI exercise incomplete: opens="+std::to_string(menu_opened));
    if(qa&&(stage!=3||saves!=1||loads!=1))throw std::runtime_error("SDL slot test incomplete");
    std::cout<<"{\"status\":\""<<(qa?"passed":"window_closed")<<"\",\"window_presentations\":"<<presented<<",\"slot_saves\":"<<saves<<",\"slot_loads\":"<<loads<<",\"ui_slot_exact\":"<<(stage==3?"true":"false")<<",\"audio_device_open\":"<<(audio.stream?"true":"false")<<",\"audio_queue_resets\":"<<audio.queue_resets<<",\"native_audio_frames\":"<<audio.emitted<<",\"bar_mouse_test\":"<<(bar_stage==4?"true":"false")<<",\"coordinate_checks\":"<<coordinate_checks<<',';machine->report();std::cout<<"}\n";
    SDL_DestroyTexture(bar_texture);SDL_DestroyTexture(texture);SDL_DestroyRenderer(r);SDL_DestroyWindow(w);return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
