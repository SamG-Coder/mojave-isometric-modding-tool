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
 std::cout<<"World aim and combat gating checks passed.\n";
}
