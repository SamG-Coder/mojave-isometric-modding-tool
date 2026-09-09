// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#include "../src/combat_math.hpp"
#include <iostream>
#include <cstdlib>
void check(bool value,const char* message){if(!value){std::cerr<<message<<"\n";std::exit(1);}}
int main(){
 auto north=combat::aim({0,0,0},{0,100,0});check(std::abs(north.yaw)<.001f,"North heading incorrect");
 auto east=combat::aim({0,0,0},{100,0,0});check(std::abs(east.yaw-1.5707963f)<.001f,"East heading incorrect");
 check(combat::aim({0,0,0},{0,100,100}).pitch<0,"Upward aim has wrong pitch sign");
 check(combat::aim({0,0,0},{0,100,-100}).pitch>0,"Downward aim has wrong pitch sign");
 check(combat::canAttack(100,200,true,false,true,true),"Clear in-range attack rejected");
 check(!combat::canAttack(300,200,true,false,true,true),"Out-of-range attack accepted");
 check(!combat::canAttack(100,200,false,false,true,true),"Blocked shot accepted");
 check(!combat::canAttack(100,200,true,true,true,true),"Moving attack accepted");
 check(!combat::canAttack(100,200,true,false,false,true),"Menu attack accepted");
 check(!combat::canAttack(100,200,true,false,true,false),"Dead target attack accepted");
 auto muzzleAim=combat::aim({20,0,70},{100,100,70});
 check(std::abs(muzzleAim.pitch)<.001f,"Level muzzle-to-torso shot aims vertically");
 check(std::abs(muzzleAim.yaw-std::atan2(80.f,100.f))<.001f,"Muzzle lateral offset ignored");
 auto crouched=combat::aim({20,0,70},{100,100,40});check(crouched.pitch>0,"Lower torso target not tracked");
 // Engagement is decided before movement is stopped, independent of a pending route.
 check(combat::canEngage(199,200,true,true,true),"In-range chase did not yield to attack");
 check(!combat::canEngage(199,200,false,true,true),"Chase fired through obstruction");
 check(combat::refreshPursuit(100,0,0,1000,true,true),"First approach waited for timer");
 check(!combat::refreshPursuit(1249,1000,500,1000,true,true),"Moving target bypassed bounded update rate");
 check(combat::refreshPursuit(1250,1000,100,1000,true,true),"Stale pending route prevented pursuit update");
 check(!combat::refreshPursuit(1250,1000,5,1000,true,false),"Minor target movement restarted pursuit");
 check(combat::refreshPursuit(1250,1000,0,1000,false,false),"Ended approach delayed fresh pursuit");
 check(!combat::refreshPursuit(1250,1000,0,1000,false,true),"Unchanged pending search restarted");
 check(combat::meleeDistance({0,0,0},{60,0,0})==60,"Melee range used weapon height");
 check(combat::meleeDistance({0,0,0},{60,0,200})==200,"Melee attacked across floors");
 check(combat::useAimDownSights(600,false,false),"Distant ranged attack did not aim");
 check(!combat::useAimDownSights(300,false,true),"Close ranged attack retained sights");
 check(combat::useAimDownSights(450,false,true)&&!combat::useAimDownSights(450,false,false),"Aim threshold oscillates");
 check(!combat::useAimDownSights(600,true,true),"Melee activated aim/block control");
 // Menu, dialogue, furniture and native POV are deliberately not ownership inputs.
 check(combat::keepCamera(true,true,false),"Loaded world lost camera ownership");
 check(!combat::keepCamera(true,false,false),"Missing world retained camera");
 check(!combat::keepCamera(false,true,false),"Disabled mod retained camera");
 check(!combat::keepCamera(true,true,true),"Explicit disable failed to release camera");
 for(uint32_t mode:{1u,2u,3u,4u})check(!combat::keepCamera(true,true,false,mode),"Pip-Boy camera not released during transition");
 check(combat::keepCamera(true,true,false,0),"Closing Pip-Boy did not restore isometric ownership");
 std::cout<<"World aim and combat gating checks passed.\n";
}
