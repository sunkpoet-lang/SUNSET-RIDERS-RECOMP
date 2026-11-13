// Sunset-specific host policy. No guest addresses or imported cheat scripts.
#pragma once
#include <cstdint>
namespace sunset_menu {
enum class Mode { LocalClean, LocalAssisted, Preparing, Online, Reconnecting, Spectating, Replay, Unknown };
enum class Action { Assist, Turbo, Save, Load, Pause, Speed, Reset, Test, Volume, Display };
class Policy {
    Mode mode_ = Mode::LocalClean;
    bool tainted_ = false;
public:
    Mode mode() const { return mode_; }
    bool tainted() const { return tainted_; }
    bool local() const { return mode_==Mode::LocalClean || mode_==Mode::LocalAssisted; }
    bool allows(Action a) const {
        if(a==Action::Volume || a==Action::Display) return true;
        return local();
    }
    void enter(Mode next) { mode_=next; }
    bool mark_applied(Action a) {
        if(!allows(a)) return false;
        if(a==Action::Assist || a==Action::Turbo) { tainted_=true; mode_=Mode::LocalAssisted; }
        return true;
    }
    // Must be backed by clean-machine construction and peer checks by the host.
    bool eligible_online_initial_state() const { return !tainted_ && mode_==Mode::Preparing; }
};
}
