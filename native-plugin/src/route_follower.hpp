// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include "camera_math.hpp"
#include <vector>
#include <algorithm>
namespace navigation {
class Follower {
public:
 std::vector<engine::Vec> points;size_t cursor{};
 void clear(){points.clear();cursor=0;}
 bool done()const{return cursor>=points.size();}
 void advance(engine::Vec pos){while(!done()&&engine::length(points[cursor]-pos)<12)++cursor;}
 template<class Clear> bool adopt(const std::vector<engine::Vec>& replacement,engine::Vec pos,Clear clear){
  if(replacement.empty())return false;
  size_t nearest=0;float best=1e30f;
  for(size_t i=0;i<replacement.size();i++){float d=engine::length(replacement[i]-pos);if(d<best){best=d;nearest=i;}}
  // Attach to the current position, not the position where planning began.
  // Only replace the active route after a valid joining corridor is found.
  size_t end=std::min(replacement.size()-1,nearest+3);
  for(size_t i=end+1;i-->nearest;){if(engine::length(replacement[i]-pos)<=256&&clear(pos,replacement[i])){
   points.assign(replacement.begin()+i,replacement.end());cursor=0;advance(pos);return true;
  }}return false;
 }
 template<class Clear> bool adjust(engine::Vec pos,engine::Vec goal,Clear clear){
  if(engine::length(goal-pos)<=192&&clear(pos,goal)){points={goal};cursor=0;return true;}
  if(!done()&&engine::length(points.back()-goal)<=128){
   float retained=engine::length(points[cursor]-pos)+engine::length(points.back()-goal);
   for(size_t i=cursor+1;i<points.size();i++)retained+=engine::length(points[i]-points[i-1]);
   // Nearby endpoints alone do not justify keeping a long old detour.
   if(retained<=engine::length(goal-pos)*1.35f+64&&clear(points.back(),goal)){points.push_back(goal);return true;}
  }
  return false;
 }
 template<class Clear> void shortcut(engine::Vec pos,Clear clear){
  if(done())return;
  size_t end=std::min(points.size()-1,cursor+3);
  for(size_t i=end;i>cursor;--i)if(engine::length(points[i]-pos)<=128&&clear(pos,points[i])){cursor=i;break;}
 }
};
}
