// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#include "../src/wheel_input.hpp"
#include <iostream>
#include <cstdlib>
void check(bool condition,const char* message){if(!condition){std::cerr<<message<<"\n";std::exit(1);}}
int main(){
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
