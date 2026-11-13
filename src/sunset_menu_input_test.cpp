#include "sunset_menu_input.h"
#include <iostream>
#include <stdexcept>
static int checks=0;static void check(bool ok){++checks;if(!ok)throw std::runtime_error("input hygiene regression");}
int main(){using namespace sunset_menu;ReleaseGate gate;
 for(unsigned p=0;p<4;++p)check(gate.accept(p,true));gate.block_all();
 for(unsigned p=0;p<4;++p){for(int i=0;i<100;++i)check(!gate.accept(p,true));check(!gate.accept(p,false));check(gate.accept(p,true));}
 gate.block(2);check(!gate.accept(2,true));check(gate.accept(1,true));check(!gate.accept(4,false));
 PadMenuInput pads;SDL_Event e{};e.type=SDL_EVENT_GAMEPAD_BUTTON_DOWN;e.gbutton.which=42;
 for(auto pair:{std::pair{SDL_GAMEPAD_BUTTON_DPAD_UP,SDL_SCANCODE_UP},std::pair{SDL_GAMEPAD_BUTTON_DPAD_DOWN,SDL_SCANCODE_DOWN},std::pair{SDL_GAMEPAD_BUTTON_DPAD_LEFT,SDL_SCANCODE_LEFT},std::pair{SDL_GAMEPAD_BUTTON_DPAD_RIGHT,SDL_SCANCODE_RIGHT},std::pair{SDL_GAMEPAD_BUTTON_SOUTH,SDL_SCANCODE_RETURN},std::pair{SDL_GAMEPAD_BUTTON_EAST,SDL_SCANCODE_BACKSPACE}}){e.gbutton.button=pair.first;check(pads.translate(e,true)==pair.second);check(pads.translate(e,false)==SDL_SCANCODE_UNKNOWN);}
 e.gbutton.button=SDL_GAMEPAD_BUTTON_BACK;check(pads.translate(e,false)==SDL_SCANCODE_ESCAPE);
 e.gbutton.button=SDL_GAMEPAD_BUTTON_START;check(pads.translate(e,true)==SDL_SCANCODE_UNKNOWN);
 e.type=SDL_EVENT_GAMEPAD_AXIS_MOTION;e.gaxis.axis=SDL_GAMEPAD_AXIS_LEFTX;e.gaxis.which=42;e.gaxis.value=20000;
 check(pads.translate(e,true)==SDL_SCANCODE_RIGHT);check(pads.translate(e,true)==SDL_SCANCODE_UNKNOWN);
 e.gaxis.value=14000;check(pads.translate(e,true)==SDL_SCANCODE_UNKNOWN);e.gaxis.value=0;check(pads.translate(e,true)==SDL_SCANCODE_UNKNOWN);
 e.gaxis.value=-20000;check(pads.translate(e,true)==SDL_SCANCODE_LEFT);
 e.gaxis.which=43;e.gaxis.axis=SDL_GAMEPAD_AXIS_LEFTY;check(pads.translate(e,true)==SDL_SCANCODE_UP);
 pads.remove(43);check(pads.translate(e,true)==SDL_SCANCODE_UP);pads.clear();
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"physical_controller_acceptance\":false}\n";
}
