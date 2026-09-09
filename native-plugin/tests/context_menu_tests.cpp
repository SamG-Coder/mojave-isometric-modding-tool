// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#include "../src/context_menu_model.hpp"
#include <cstdlib>
#include <iostream>
using namespace context_menu;
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<"\n";std::exit(1);}}
bool has(const std::vector<Row>& rows,Action a){for(auto& r:rows)if(r.action==a)return true;return false;}
int main(){
 auto living=actions(0x2A,false,true,true);check(living.size()==6&&living[0].label=="Talk"&&has(living,Action::Attack)&&has(living,Action::Vats),"Living actor actions incorrect");
 for(auto& row:living)check(row.label!="Stop","Redundant Stop option remains");
 check(has(actions(0,false,false,false),Action::PickupArea),"Ground has no area pickup action");
 check(inPickupArea({100,0,0},{0,0,0},{0,0,0}),"Nearby pickup omitted");
 check(!inPickupArea({100,0,180},{0,0,0},{0,0,0}),"Other floor included");
 check(!inPickupArea({300,0,0},{0,0,0},{0,0,0}),"Outside pickup radius included");
 auto small=layout({100,100},{1706,960},5,24,150),large=layout({100,100},{1706,960},5,52,620);
 check(large.width>=652&&large.width>small.width,"Backdrop does not expand for wider text");
 check(large.height()>small.height()&&large.rowHeight>=60,"Backdrop does not expand for taller text");
 auto fewer=layout({100,100},{1706,960},2,24,150);check(fewer.height()<small.height(),"Backdrop retains removed rows");
 auto dead=actions(0x2A,true,false,true);check(dead[0].label=="Search"&&!has(dead,Action::Attack)&&!has(dead,Action::Vats),"Corpse offers combat");
 for(unsigned k:{0x1Bu,0x1Cu,0x27u,0x17u,0x28u,0x29u,0x31u,0x26u}){auto r=actions(k,false,false,true);check(has(r,Action::Activate)&&!has(r,Action::Attack)&&r.size()<=6,"Object activation missing");}
 check(actions(0x28,false,false,true)[0].label=="Take","Weapon cannot be picked up");
 check(actions(0x27,false,false,true)[0].label=="Sit / use","Furniture verb incorrect");
 for(bool object:{false,true}){auto r=actions(0x20,false,false,object);check(!has(r,Action::Activate)&&!has(r,Action::Attack)&&has(r,Action::Move),"Scenery offers invalid action");}
 for(auto screen:{engine::ScreenPoint{1024,768},engine::ScreenPoint{2560,1440},engine::ScreenPoint{3440,1440},engine::ScreenPoint{3840,2160}}){
  for(float menuHeight:{720.f,960.f,1200.f}){
   auto extent=engine::nativeMenuExtent(screen.x,screen.y,menuHeight/screen.y);engine::UITransform transform{screen.x,screen.y,extent.x,extent.y};
   for(auto click:{engine::ScreenPoint{0,0},engine::ScreenPoint{screen.x-1,screen.y-1},engine::ScreenPoint{screen.x*.5f,screen.y*.5f}}){
    auto l=layout(transform.toUI(click),extent,6,36,340);
    check(l.x>=0&&l.y>=0&&l.x+l.width<=extent.x&&l.y+l.height()<=extent.y,"Context menu outside native viewport");
    check(l.hit({l.x+10,l.y+5})==-1&&l.hit({l.x-1,l.y+l.header+1})==-1,"Title/outside click selected action");
    for(int i=0;i<6;++i){auto pixel=transform.toPixels({l.x+l.width*.5f,l.y+l.header+(i+.5f)*l.rowHeight});check(l.hit(transform.toUI(pixel))==i,"Scaled row hitbox mismatch");}
    check(l.hit({l.x+1,l.y+l.header+l.rowHeight*6})==-1,"Bottom edge selects nonexistent row");
   }
  }
 }
 std::cout<<"Context actions, edge placement and scaled row hitboxes passed.\n";
}
