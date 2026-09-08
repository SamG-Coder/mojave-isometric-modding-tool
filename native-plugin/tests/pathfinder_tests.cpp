// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#include "../src/pathfinder.hpp"
#include <iostream>
#include <cstdlib>
using engine::Vec;using navigation::Search;
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<"\n";std::exit(1);}}
int main(){
    auto floor=[](Vec p,Vec& out){out={p.x,p.y,0};return std::abs(p.x)<=640&&std::abs(p.y)<=640;};
    auto wall=[](Vec a,Vec b){for(int i=0;i<=20;i++){Vec p=a+(b-a)*(i/20.f);if(p.x>100&&p.x<156&&std::abs(p.y)<150)return false;}return true;};
    Search search;search.begin({0,0,0},{320,0,0},20);
    for(int i=0;i<200&&search.state==Search::State::Searching;i++)search.step(floor,wall,2);
    check(search.state==Search::State::Found,"A* failed to find route around wall");bool detour=false;
    for(size_t i=1;i<search.path.size();i++){check(wall(search.path[i-1],search.path[i]),"Route crosses wall");if(std::abs(search.path[i].y)>=150)detour=true;}check(detour,"Route did not detour");
    search.begin({0,0,0},{320,0,0},20);for(int i=0;i<10&&search.state==Search::State::Searching;i++)search.step(floor,[](Vec,Vec){return false;},4);check(search.state==Search::State::NoPath,"Unreachable target did not fail");
    search.begin({0,0,0},{320,0,0},20);search.cancel();check(search.state==Search::State::Idle&&search.path.empty(),"Cancellation left stale waypoints");
    auto steep=[](Vec p,Vec& out){out=p;out.z=p.x>0?200:0;return std::abs(p.x)<400&&std::abs(p.y)<400;};search.begin({0,0,0},{320,0,200},20);
    for(int i=0;i<200&&search.state==Search::State::Searching;i++)search.step(steep,[](Vec,Vec){return true;},4);check(search.state==Search::State::NoPath,"A* climbed an impassable step");
    std::cout<<"A* detour, collision clearance, unreachable target, cancellation and step-height tests passed.\n";
}
