// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#include "../src/wheel_input.hpp"
#include "../src/input_ownership.hpp"
#include <array>
#include <iostream>
#include <cstdlib>
void check(bool condition,const char* message){if(!condition){std::cerr<<message<<"\n";std::exit(1);}}
int main(){
    for(int control:{4,6,13}){
        check(input_ownership::command(true,control)=="DisableControl "+std::to_string(control)+" 1","Ownership must suppress physical input only");
        check(input_ownership::command(false,control)=="EnableControl "+std::to_string(control)+" 1","Release must not change script disable state");
    }
    // xNVSE returns the final merged state. Exercise single fire taps, held
    // automatic fire/ADS, releases and both native mouse buffer sizes.
    for(unsigned buttons:{4u,8u})for(unsigned char pressed:{0u,0x80u})for(bool owns:{false,true}){
        struct State {long x=14,y=-7,wheel=120;std::array<unsigned char,8> buttons{};} state;
        state.buttons.fill(pressed);auto before=state.buttons;
        WheelInput filter;std::atomic<int> x{},y{};
        filter.filterMotion(state.x,state.y,state.wheel,owns,owns,x,y);
        for(unsigned i=0;i<buttons;++i)check(state.buttons[i]==before[i],"Mouse filtering erased a scripted attack/aim or changed release");
        check(state.x==(owns?0:14)&&state.y==(owns?0:-7),"Motion ownership changed");
    }
    WheelInput input;long wheel=-360;
    input.filter(wheel,true);check(wheel==0,"Vanilla camera must not receive owned zoom");
    check(input.take()==-360,"Preserve scroll magnitude and direction");check(input.take()==0,"Consume once");
    wheel=120;input.filter(wheel,true);wheel=240;input.filter(wheel,true);check(input.take()==360,"Accumulate device polls");
    wheel=120;input.filter(wheel,true);input.reset();check(input.take()==0,"No wheel survives save-load reset");
    wheel=120;input.filter(wheel,true);wheel=-240;input.filter(wheel,false);
    check(wheel==-240,"Menu and normal camera scrolling must pass through");check(input.take()==0,"No stale zoom on menu return");
    wheel=120;input.filter(wheel,true);check(input.take()==120,"Zoom resumes after menu or save load");
    std::cout<<"Wheel ownership, magnitude, menu handoff and save-load reset checks passed.\n";
}
