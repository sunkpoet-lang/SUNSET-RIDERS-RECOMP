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
            if(action.id==sunset_menu::BarModel::About)status="Laboratorio EAC: CPU interpretadas, 5 min, sin online.";
            if(action.id==sunset_menu::BarModel::Shortcuts)status="Esc menu; F10 barra; F5 guardar; F8 cargar; F11 pantalla.";
        }
        sync_sound();refresh_bar();
    };
    auto opened_bar=[&]{bar_prior_pause=paused&&!overlay.menu.IsOpen();overlay.menu.Close();refresh_bar();if(overlay.policy.local())paused=true;hush();};
    auto update_bar_pause=[&]{if(!bar.IsOpen()){paused=bar_prior_pause||overlay.menu.IsOpen();hush();}};
    auto chose_bar=[&](recomp::MenuBar::Chosen choice){if(choice.chosen){dispatch(bar_model.choose(choice.id));bar.Close();}update_bar_pause();};
    auto click_logical=[&](float x,float y){float wx,wy,lx,ly;sdl_require(SDL_RenderCoordinatesToWindow(r,x,y,&wx,&wy),"logical mouse conversion");sdl_require(SDL_RenderCoordinatesFromWindow(r,wx,wy,&lx,&ly),"window mouse conversion");if(std::abs(lx-x)>.05f||std::abs(ly-y)>.05f)throw std::runtime_error("letterbox coordinates diverged");++coordinate_checks;SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.windowID=SDL_GetWindowID(w);e.button.button=SDL_BUTTON_LEFT;e.button.x=wx;e.button.y=wy;SDL_PushEvent(&e);};
    auto*bar_texture=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,576,536);sdl_require(bar_texture!=nullptr,"bar texture");SDL_SetTextureBlendMode(bar_texture,SDL_BLENDMODE_NONE);
