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
