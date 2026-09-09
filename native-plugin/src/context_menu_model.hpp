// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#pragma once
#include "camera_math.hpp"
#include <algorithm>
#include <string>
#include <vector>
namespace context_menu {
enum class Action {Activate,Attack,Vats,Move,Stop,Cancel};
struct Row {Action action;std::string label;};
inline bool pickup(unsigned k){return k==0x18||k==0x19||k==0x1D||k==0x1F||k==0x28||k==0x29||k==0x2E||k==0x2F||k==0x31;}
inline bool activatable(unsigned k){return pickup(k)||k==0x15||k==0x16||k==0x17||k==0x1B||k==0x1C||k==0x26||k==0x27||k==0x2A||k==0x2B;}
inline const char* verb(unsigned k,bool dead){
 if(k==0x2A||k==0x2B)return dead?"Search":(k==0x2A?"Talk":"Interact");
 if(k==0x1C)return "Open / close";
 if(k==0x1B)return "Open";
 if(k==0x27)return "Sit / use";
 if(k==0x26)return "Harvest";
 if(pickup(k))return "Take";
 return "Use";
}
inline std::vector<Row> actions(unsigned kind,bool dead,bool attackable,bool object){
 std::vector<Row> r;
 if(object&&activatable(kind))r.push_back({Action::Activate,verb(kind,dead)});
 if(object&&attackable){r.push_back({Action::Attack,"Attack"});r.push_back({Action::Vats,"Open VATS"});}
 r.push_back({Action::Move,object?"Walk here":"Move here"});
 r.push_back({Action::Stop,"Stop"});r.push_back({Action::Cancel,"Cancel"});return r;
}
struct Layout {
 float x{},y{},width{},header{},rowHeight{};size_t count{};
 float height()const{return header+rowHeight*float(count)+8;}
 int hit(engine::ScreenPoint p)const{
  if(p.x<x||p.x>=x+width||p.y<y+header||p.y>=y+header+rowHeight*float(count))return -1;
  return int((p.y-y-header)/rowHeight);
 }
};
inline Layout layout(engine::ScreenPoint p,engine::ScreenPoint extent,size_t count,float textHeight,float textWidth){
 Layout r;r.width=std::min(std::max(260.f,textWidth+32),extent.x-16);r.header=std::max(40.f,textHeight+12);r.rowHeight=std::max(34.f,textHeight+8);r.count=count;
 r.x=std::clamp(p.x+12,8.f,std::max(8.f,extent.x-r.width-8));r.y=std::clamp(p.y+12,8.f,std::max(8.f,extent.y-r.height()-8));return r;
}
}
