// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#include "../src/route_follower.hpp"
#include "../src/pathfinder.hpp"
#include <iostream>
#include <cstdlib>
using engine::Vec;
void check(bool v,const char* s){if(!v){std::cerr<<s<<"\n";std::exit(1);}}
int main(){
 auto clear=[](Vec,Vec){return true;};auto blocked=[](Vec,Vec){return false;};
 navigation::Follower f;f.points={{32,0,0},{64,0,0},{96,0,0},{128,0,0}};
 navigation::Search pending;pending.begin({0,0,0},{512,0,0},20);
 check(f.points.size()==4&&!f.done(),"Starting replacement destroyed active route");
 f.advance({32,0,0});check(f.cursor==1,"Active route did not advance while replacement pending");
 check(!f.adopt({{100,100,0},{200,100,0}},{64,0,0},blocked)&&f.points.size()==4&&f.cursor==1,"Invalid join replaced safe route");
 check(f.adopt({{0,0,0},{32,0,0},{64,0,0},{96,0,0},{128,0,0}},{90,0,0},clear),"Moving-position join failed");
 check(!f.done()&&f.points[f.cursor].x>=90,"Replacement sends player back to search origin");
 auto old=f.points;check(!f.adjust({90,0,0},{300,100,0},blocked)&&f.points.size()==old.size(),"Blocked adjustment discarded active route");
 check(f.adjust({90,0,0},{160,0,0},clear)&&f.points.back().x==160,"Small destination adjustment failed");
 f.points={{32,0,0},{32,32,0},{64,32,0}};f.cursor=0;f.shortcut({0,0,0},blocked);check(f.cursor==0,"Shortcut cut blocked corner");
 f.shortcut({0,0,0},clear);check(f.cursor==2,"Clear route was not shortened");
 f.points={{0,500,0},{500,500,0},{500,0,0}};f.cursor=0;
 check(!f.adjust({0,0,0},{510,0,0},clear)&&f.points.size()==3,"Nearby click retained an excessive old detour");
 f.points={{250,0,0},{500,0,0}};f.cursor=0;
 check(f.adjust({0,0,0},{510,0,0},clear),"Efficient route extension was rejected");
 f.clear();check(f.done(),"Explicit cancel kept route active");
 std::cout<<"Concurrent route retention, moving joins, reuse, blocked joins and shortcut tests passed.\n";
}
