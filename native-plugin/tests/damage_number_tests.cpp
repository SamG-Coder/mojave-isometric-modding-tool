// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#include "../src/damage_numbers.hpp"
#include <cstdlib>
#include <limits>
#include <iostream>
void check(bool value){if(!value)std::exit(1);}
int main(){
 using namespace damage_numbers;
 check(lostHealth(100,75)==25);
 check(lostHealth(10,-90)==10);
 check(lostHealth(-10,-20)==0);
 check(lostHealth(75,100)==0);
 check(lostHealth(100,100)==0);
 check(lostHealth(std::numeric_limits<float>::quiet_NaN(),0)==0);
 check(opacity(0)==1&&opacity(.8f)==1&&opacity(1.2f)>0&&opacity(1.4f)==0);
 std::cout<<"Damage loss, overkill, healing, invalid values and popup lifetime passed.\n";
}
