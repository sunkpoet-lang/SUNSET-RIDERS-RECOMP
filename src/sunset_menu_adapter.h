#pragma once
#include "sunset_menu_policy.h"
#include "sunset_turbo.h"
#include "sunset_assists.h"
#include "menu.h"
#include "menubar.h"
#include <algorithm>
namespace sunset_menu {
class Adapter {
public:
    enum { Fullscreen=1, Controls, Cheats, OnlineInfo };
    recomp::Menu menu;recomp::MenuConfig config;
    Policy policy;
    Turbo turbo;
    Assists assists;
    int volume=50;
    bool muted=false;
    void open() {
        recomp::MenuConfig c;
        c.groups={{{recomp::MenuRow::Resume}},
          {{recomp::MenuRow::Save},{recomp::MenuRow::Load}},
          {{recomp::MenuRow::Mute},
           {recomp::MenuRow::Action,Fullscreen,"Fullscreen","Toggle the local display."},
           {recomp::MenuRow::Action,Controls,"Four player controls","Separate keyboard layouts and connected gamepads."},
           {recomp::MenuRow::Page,0,"Cheats","Offline lives and one-shot native powerups; EAC only."},
           {recomp::MenuRow::Action,OnlineInfo,"Online (not available yet)","Four-player transport and clean session admission remain gated."}},
          {{recomp::MenuRow::Quit}}};
        c.groups.insert(c.groups.end()-1,{{recomp::MenuRow::Page,0,"Turbo fire"}});
        for(unsigned p=0;p<4;++p)c.features.push_back({"Turbo fire","Player "+std::to_string(p+1),"Offline fire pulses per simulated second. Hold fire; jump/coin unchanged.",{"Off","5 / sec","10 / sec","15 / sec"}});
        c.pages.push_back({"Cheats","","Offline only. Lives are not invincibility; powerups are given once.",false});
        for(unsigned p=0;p<4;++p){std::string page="Player "+std::to_string(p+1)+" cheats";
            c.pages.push_back({page,"Cheats","Requires an active player in normal gameplay.",false});
            c.features.push_back({page,"Infinite lives","Keeps at least three lives while active. Does not prevent damage.",{"Off","On"}});
            c.commands.push_back({page,"Give rapid fire","One native pickup; survives only as the game normally allows."});
            c.commands.push_back({page,"Give double guns","One native pickup, including the game's animation update."});
            c.commands.push_back({page,"Give both powerups","Give missing powerups once. Busy/inactive requests may be cancelled."});
        }
        c.value=[this](size_t p){return p<4?int(turbo.rate(unsigned(p))/5):int(assists.infinite(unsigned(p-4)));};
        c.setValue=[this](size_t p,int v){if(p<4&&v>=0&&v<=3)turbo.set(unsigned(p),unsigned(v)*5,policy);else if(p>=4&&p<8&&(v==0||v==1))assists.set_lives(unsigned(p-4),v!=0,policy);};
        c.offered=[this](size_t,int v){return v==0||policy.allows(Action::Assist);};
        c.confirmName="Enter / A";c.closeName="Esc / B";
        c.muted=[this]{return muted;};c.setMuted=[this](bool v){if(policy.allows(Action::Volume))muted=v;};
        c.volume=[this]{return volume;};c.setVolume=[this](int v){if(policy.allows(Action::Volume))volume=std::clamp(v,0,100);};
        config=c;menu.Configure(std::move(c));menu.Open();
        menu.Note("Temporary own-session slot; lost on exit. Local experimental session.");
    }
    recomp::Menu::Chosen press(unsigned buttons){return handle(menu.Buttons(buttons));}
    recomp::Menu::Chosen choose(int id){return handle(recomp::ChooseMenuEntry(id,config,menu));}
    recomp::Menu::Chosen handle(recomp::Menu::Chosen choice) {
        if(choice.kind==recomp::Menu::Chosen::Command){unsigned command=unsigned(choice.id);bool ok=command<12&&assists.request(command/3,command%3+1,policy);menu.Note(ok?"Queued. Resume to deliver; active normal player required.":"Powerups unavailable in this session/profile.");return {};}

        if((choice.kind==recomp::Menu::Chosen::Save&&!policy.allows(Action::Save)) ||
           (choice.kind==recomp::Menu::Chosen::Load&&!policy.allows(Action::Load))) {
            menu.Note("States are unavailable in this session mode.");return {};
        }
        if(choice.kind==recomp::Menu::Chosen::Action && (choice.id==Cheats||choice.id==OnlineInfo)) {
            menu.Note(choice.id==Cheats?"No verified cheats enabled. Offline only.":"Online is not implemented in this laboratory.");return {};
        }
        return choice;
    }
};
}
