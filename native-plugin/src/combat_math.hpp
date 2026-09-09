// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include "camera_math.hpp"
#include <algorithm>
#include <cstdint>
namespace combat {
struct ShotHold {
 uint64_t started{};bool pending{},observedAttack{};
 void begin(uint64_t now){started=now;pending=true;observedAttack=false;}
 bool update(uint64_t now,bool attacking){
  if(!pending)return false;observedAttack|=attacking;
  if(now-started>=1500||(observedAttack&&!attacking&&now-started>=150))pending=false;
  return pending;
 }
};
// Require a continuous observed native aim state, not merely submitted input.
struct SightsGate {
 uint64_t since{};bool observed{};
 void reset(){since=0;observed=false;}
 bool ready(uint64_t now,bool requested,bool actual){
  if(!requested){reset();return true;}
  if(!actual){reset();return false;}
  if(!observed){observed=true;since=now;}
  return now-since>=120;
 }
};
struct Aim {float yaw,pitch;};
inline engine::Vec direction(Aim a){float c=std::cos(a.pitch);return {std::sin(a.yaw)*c,std::cos(a.yaw)*c,-std::sin(a.pitch)};}
inline Aim aim(engine::Vec from,engine::Vec to){auto d=to-from;return {std::atan2(d.x,d.y),-std::atan2(d.z,std::max(.001f,std::hypot(d.x,d.y)))};}
inline float meleeHeading(engine::Vec feet,engine::Vec target,float current,float dt){
 auto d=target-feet;if(std::hypot(d.x,d.y)<12.f)return current;
 float delta=std::remainder(std::atan2(d.x,d.y)-current,6.283185307f);
 float step=6.283185307f*std::clamp(dt,0.f,.05f);
 return current+std::clamp(delta,-step,step);
}
inline bool meleeFacing(engine::Vec feet,engine::Vec target,float yaw){
 auto d=target-feet;if(std::hypot(d.x,d.y)<12.f)return true;
 return std::abs(std::remainder(std::atan2(d.x,d.y)-yaw,6.283185307f))<.436332313f;
}
inline float meleeDistance(engine::Vec feet,engine::Vec targetFeet){auto d=targetFeet-feet;return std::max(std::hypot(d.x,d.y),std::abs(d.z));}
inline bool useAimDownSights(float distance,bool melee,bool alreadyAiming){return !melee&&distance>(alreadyAiming?400.f:500.f);}
inline bool startupCameraReady(bool requested,bool enabled,bool hooks,bool armed,bool worldReady){
 return requested&&!enabled&&hooks&&armed&&worldReady;
}
inline bool keepCamera(bool enabled,bool loadedWorld,bool disabling,uint32_t pipboy=0){return enabled&&loadedWorld&&!disabling&&pipboy==0;}
inline bool restoreThirdPersonBody(bool cameraOwned,bool gameplay,bool talking,bool sitting,bool alreadyThird){
 return cameraOwned&&!alreadyThird&&(talking||(gameplay&&!sitting));
}
// Obstruction is advisory for projectiles; native collision decides what is hit.
// Melee still requires an unobstructed reach to avoid swinging through walls.
inline bool canEngage(float distance,float range,bool clear,bool gameplay,bool alive,bool melee){return gameplay&&alive&&(!melee||clear)&&distance<=range;}
inline bool refreshPursuit(uint64_t now,uint64_t previous,float targetShift,float range,bool moving,bool searching){
 if(!previous)return true;
 if(now-previous<250)return false;
 return targetShift>=std::clamp(range*.1f,32.f,128.f)||(!moving&&!searching);
}
inline bool canAttack(float distance,float range,bool clear,bool moving,bool gameplay,bool alive,bool melee){return !moving&&canEngage(distance,range,clear,gameplay,alive,melee);}
}
