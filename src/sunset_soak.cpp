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
