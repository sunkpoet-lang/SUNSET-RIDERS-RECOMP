#include "sunset_menu_adapter.h"
#include <iostream>
#include <stdexcept>
#include <vector>
static unsigned checks=0;
static void check(bool ok){++checks;if(!ok)throw std::runtime_error("menu/policy regression");}
int main(){using namespace sunset_menu;
    Policy p;
    for(auto mode:{Mode::LocalClean,Mode::LocalAssisted,Mode::Preparing,Mode::Online,Mode::Reconnecting,Mode::Spectating,Mode::Replay,Mode::Unknown}) {
        p.enter(mode);
        for(auto a:{Action::Assist,Action::Turbo,Action::Save,Action::Load,Action::Pause,Action::Speed,Action::Reset,Action::Test})
            check(p.allows(a)==(mode==Mode::LocalClean||mode==Mode::LocalAssisted));
        check(p.allows(Action::Volume));check(p.allows(Action::Display));
    }
    Policy taint;check(taint.mark_applied(Action::Assist));taint.enter(Mode::Preparing);
    check(!taint.eligible_online_initial_state());check(!taint.mark_applied(Action::Turbo));check(taint.tainted());
    Adapter a;a.open();check(a.menu.IsOpen());a.press(recomp::kMenuDown);
    check(a.press(recomp::kMenuConfirm).kind==recomp::Menu::Chosen::Save);
    a.policy.enter(Mode::Online);check(a.press(recomp::kMenuConfirm).kind==recomp::Menu::Chosen::None);
    a.press(recomp::kMenuDown);check(a.press(recomp::kMenuConfirm).kind==recomp::Menu::Chosen::None);
    a.policy.enter(Mode::LocalClean);check(a.press(recomp::kMenuConfirm).kind==recomp::Menu::Chosen::Load);
    std::vector<uint32_t> image(576*448,0xff302010);auto old=image;a.menu.Draw(image.data(),576,448);check(image!=old);
    a.press(recomp::kMenuBack);check(!a.menu.IsOpen());
    std::cout<<"{\"checks\":"<<checks<<",\"status\":\"passed\",\"window_integrated\":false}\n";
}
