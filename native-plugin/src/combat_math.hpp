// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include "camera_math.hpp"
#include <algorithm>
#include <cstdint>
namespace combat {
struct Aim {float yaw,pitch;};
inline Aim aim(engine::Vec from,engine::Vec to){auto d=to-from;return {std::atan2(d.x,d.y),-std::atan2(d.z,std::max(.001f,std::hypot(d.x,d.y)))};}
inline float meleeDistance(engine::Vec feet,engine::Vec targetFeet){auto d=targetFeet-feet;return std::max(std::hypot(d.x,d.y),std::abs(d.z));}
inline bool useAimDownSights(float distance,bool melee,bool alreadyAiming){return !melee&&distance>(alreadyAiming?400.f:500.f);}
inline bool keepCamera(bool enabled,bool loadedWorld,bool disabling,uint32_t pipboy=0){return enabled&&loadedWorld&&!disabling&&pipboy==0;}
inline bool canEngage(float distance,float range,bool clear,bool gameplay,bool alive){return gameplay&&alive&&clear&&distance<=range;}
inline bool refreshPursuit(uint64_t now,uint64_t previous,float targetShift,float range,bool moving,bool searching){
 if(!previous)return true;
 if(now-previous<250)return false;
 return targetShift>=std::clamp(range*.1f,32.f,128.f)||(!moving&&!searching);
}
inline bool canAttack(float distance,float range,bool clear,bool moving,bool gameplay,bool alive){return gameplay&&alive&&!moving&&clear&&distance<=range;}
}
