#pragma once
#include <array>
#include <cstdint>
#include <unordered_map>
#include <SDL3/SDL.h>
namespace sunset_menu {
// Host-only input hygiene, deliberately absent from guest save states.
class ReleaseGate {
    std::array<bool,4> blocked_{};
public:
    void block_all(){blocked_.fill(true);}
    void block(unsigned p){if(p<4)blocked_[p]=true;}
    bool accept(unsigned p,bool held){
        if(p>=4)return false;
        if(!blocked_[p])return true;
        if(!held)blocked_[p]=false;
        return false; // The neutral sample never generates a fresh action.
    }
};
class PadMenuInput {
    std::unordered_map<SDL_JoystickID,std::array<int,2>> axes_;
public:
    void clear(){axes_.clear();}
    void remove(SDL_JoystickID id){axes_.erase(id);}
    SDL_Scancode translate(const SDL_Event&e,bool open){
        if(e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN){
            auto b=e.gbutton.button;
            if(b==SDL_GAMEPAD_BUTTON_BACK)return SDL_SCANCODE_ESCAPE;
            if(!open)return SDL_SCANCODE_UNKNOWN;
            switch(b){
              case SDL_GAMEPAD_BUTTON_DPAD_UP:return SDL_SCANCODE_UP;
              case SDL_GAMEPAD_BUTTON_DPAD_DOWN:return SDL_SCANCODE_DOWN;
              case SDL_GAMEPAD_BUTTON_DPAD_LEFT:return SDL_SCANCODE_LEFT;
              case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:return SDL_SCANCODE_RIGHT;
              case SDL_GAMEPAD_BUTTON_SOUTH:return SDL_SCANCODE_RETURN;
              case SDL_GAMEPAD_BUTTON_EAST:return SDL_SCANCODE_BACKSPACE;
              default:return SDL_SCANCODE_UNKNOWN;
            }
        }
        if(e.type!=SDL_EVENT_GAMEPAD_AXIS_MOTION||e.gaxis.axis>SDL_GAMEPAD_AXIS_LEFTY)return SDL_SCANCODE_UNKNOWN;
        auto& old=axes_[e.gaxis.which][e.gaxis.axis];int value=e.gaxis.value;
        int next=value>16000?1:value<-16000?-1:(value>-8000&&value<8000?0:old);
        bool changed=next!=old;old=next;
        if(!open||!changed||!next)return SDL_SCANCODE_UNKNOWN;
        return e.gaxis.axis==SDL_GAMEPAD_AXIS_LEFTX?(next<0?SDL_SCANCODE_LEFT:SDL_SCANCODE_RIGHT):(next<0?SDL_SCANCODE_UP:SDL_SCANCODE_DOWN);
    }
};
}
