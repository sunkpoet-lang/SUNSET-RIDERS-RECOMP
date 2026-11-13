#pragma once
#include "sunset_menu_adapter.h"
#include "menubar.h"
namespace sunset_menu {
class BarModel {
    Adapter&a;
public:
    enum {About=200,Shortcuts=201};
    explicit BarModel(Adapter&adapter):a(adapter){}
    std::vector<recomp::BarMenu> entries(bool has_slot,bool can_save){
        auto c=a.config;
        for(auto&group:c.groups)for(auto&row:group){
            if(row.kind==recomp::MenuRow::Mute||row.id==Adapter::Fullscreen&&row.kind==recomp::MenuRow::Action)row.bar="Video";
            if(row.kind==recomp::MenuRow::Action&&row.id==Adapter::Controls)row.bar="Input";
        }
        std::vector<recomp::BarMenu> menus={{"Game",{}},{"Video",{}},{"Input",{}},{"Features",{}},{"Help",{}}};
        recomp::BarOptions options;options.slotUsed=[=](int slot){return slot==1&&has_slot;};
        a.menu.SetSlot(1);recomp::AddMenuEntries(menus,c,a.menu,options);
        for(auto&menu:menus)for(auto&e:menu.entries){
            if(e.label=="Save state"||e.label=="Load state"){
                bool saving=e.label=="Save state";if(e.children.size()>1)e.children.resize(1);
                e.label=saving?"Save temporary state":"Load temporary state";e.shortcut=saving?"F5":"F8";
                e.enabled=a.policy.allows(saving?Action::Save:Action::Load)&&(saving?can_save:has_slot);
                for(auto&child:e.children){child.label="Own-session slot (lost on exit)";child.enabled=e.enabled;}
            }
            if(e.label=="Online (not available yet)")e.enabled=false;
            if(menu.title=="Features"&&!a.policy.local())e.enabled=false;
        }
        menus[4].entries={{"About this laboratory","",recomp::BarEntry::NoMark,true,About},
                          {"Shortcuts","",recomp::BarEntry::NoMark,true,Shortcuts},
                          {"Local experimental session","",recomp::BarEntry::NoMark,false},
                          {"Four-player online is not available","",recomp::BarEntry::NoMark,false}};
        return menus;
    }
    recomp::Menu::Chosen choose(int id){
        if(id==About||id==Shortcuts)return {recomp::Menu::Chosen::Action,id};
        auto choice=a.choose(id);a.menu.SetSlot(1);return choice;
    }
};
}
