// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#include "../src/lighting_distance.hpp"
#include <iostream>
#include <cstdlib>
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<"\n";std::exit(1);}}
int main(){
 for(float span:{300.f,1500.f,5000.f})for(float pitch:{20.f,50.f,80.f})for(float aspect:{1.f,16.f/9,21.f/9}){
  float angle=pitch*.01745329252f,distance=std::max(1100.f,span/aspect*.5f/std::tan(angle)+500.f);
  float cover=lighting::visibleDistance(distance,span,aspect,pitch);
  for(float side:{-1.f,1.f})for(float depth:{-1.f,1.f}){
   float x=side*span*.5f,y=depth*span/aspect*.5f/std::sin(angle);
   float actual=std::sqrt(x*x+(y+std::cos(angle)*distance)*(y+std::cos(angle)*distance)+std::pow(std::sin(angle)*distance,2));
   check(actual<cover,"Visible ground corner fades");
  }
 }
 lighting::FadeOverride fade;float start=1000,end=1200;
 fade.update(start,end,true,5000);check(start==5000&&end==5200,"Coverage/fade width incorrect");
 for(int i=0;i<1000;++i)fade.update(start,end,true,5000);check(start==5000,"Override compounded");
 fade.update(start,end,true,3000);check(start==3000&&end==3200,"Zoom-in cannot lower override");
 fade.update(start,end,false,0);check(start==1000&&end==1200,"Normal camera baseline not restored");
 fade.update(start,end,true,5000);start=2000;end=2300;fade.update(start,end,true,5000);fade.update(start,end,false,0);check(start==2000&&end==2300,"Menu change lost");
 fade.update(start,end,true,5000);start=7000;end=7500;fade.update(start,end,false,0);check(start==7000&&end==7500,"External renderer change overwritten");
 fade.update(start,end,true,3000);check(start==7000&&end==7500,"Higher configured distance reduced");
 check(lighting::visibleDistance(1000,1500,0,50)==0,"Invalid aspect accepted");
 std::cout<<"Lighting footprint, zoom, baseline restoration and external changes passed.\n";
}
