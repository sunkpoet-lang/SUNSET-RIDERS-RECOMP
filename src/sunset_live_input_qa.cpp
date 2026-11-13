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
