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
    for(int i=0;i<3000&&search.state==Search::State::Searching;i++)search.step(floor,wall,2);
    check(search.state==Search::State::Found,"A* failed to find route around wall");bool detour=false;
    for(size_t i=1;i<search.path.size();i++){check(wall(search.path[i-1],search.path[i]),"Route crosses wall");if(std::abs(search.path[i].y)>=150)detour=true;}check(detour,"Route did not detour");
    search.begin({0,0,0},{320,0,0},20);for(int i=0;i<10&&search.state==Search::State::Searching;i++)search.step(floor,[](Vec,Vec){return false;},4);check(search.state==Search::State::NoPath,"Unreachable target did not fail");
    search.begin({0,0,0},{320,0,0},20);search.cancel();check(search.state==Search::State::Idle&&search.path.empty(),"Cancellation left stale waypoints");
    auto steep=[](Vec p,Vec& out){out=p;out.z=p.x>0?200:0;return std::abs(p.x)<400&&std::abs(p.y)<400;};search.begin({0,0,0},{320,0,200},20);
    for(int i=0;i<3000&&search.state==Search::State::Searching;i++)search.step(steep,[](Vec,Vec){return true;},4);check(search.state==Search::State::NoPath,"A* climbed an impassable step");
    search.begin({0,0,0},{0,0,100},20);
    search.step(floor,[](Vec,Vec){return false;},1);
    check(search.state!=Search::State::Found,"Different floor directly overhead was treated as arrival");
    search.begin({0,0,0},{256,0,0},20);search.step(floor,[](Vec,Vec){return true;},12,100);
    check(search.state==Search::State::Found&&search.directRoute&&search.expanded==0,"Clear short route waited for A*");
    auto gap=[](Vec p,Vec& out){out={p.x,p.y,0};return !(p.x>=96&&p.x<=160&&std::abs(p.y)<64)&&std::abs(p.x)<640&&std::abs(p.y)<640;};
    search.begin({0,0,0},{256,0,0},20);
    for(int i=0;i<1000&&search.state==Search::State::Searching;i++)search.step(gap,wall,8);
    check(!search.directRoute,"Direct corridor crossed a floor gap");
    // A 42-unit doorway centred between the old 64-unit grid rows.
    auto door=[](Vec a,Vec b){for(int i=0;i<=32;i++){Vec q=a+(b-a)*(i/32.f);if(q.x>=96&&q.x<=128&&std::abs(q.y-32)>21)return false;}return true;};
    search.begin({0,0,0},{256,0,0},20);
    for(int i=0;i<1000&&search.state==Search::State::Searching;i++)search.step(floor,door,8);
    check(search.state==Search::State::Found,"Narrow doorway between coarse grid rows was missed");
    for(size_t i=1;i<search.path.size();i++)check(door(search.path[i-1],search.path[i]),"Doorway path clips wall");
    check(search.cacheHits>0,"Collision samples were not reused");
    auto queries=search.groundQueries+search.edgeQueries;
    std::cout<<"Doorway route: "<<search.expanded<<" expansions, "<<queries<<" collision callbacks, "<<search.cacheHits<<" cache hits.\n";
    search.begin({0,0,0},{256,0,0},20);check(search.cacheHits==0&&search.edgeQueries==0,"New search reused stale collision state");
    std::cout<<"A* detour, collision clearance, unreachable target, cancellation and step-height tests passed.\n";
}
