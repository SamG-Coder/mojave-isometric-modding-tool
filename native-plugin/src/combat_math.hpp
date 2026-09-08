// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include "camera_math.hpp"
#include <algorithm>
namespace combat {
struct Aim {float yaw,pitch;};
inline Aim aim(engine::Vec from,engine::Vec to){auto d=to-from;return {std::atan2(d.x,d.y),-std::atan2(d.z,std::max(.001f,std::hypot(d.x,d.y)))};}
inline bool canAttack(float distance,float range,bool clear,bool moving,bool gameplay,bool alive){return gameplay&&alive&&!moving&&clear&&distance<=range;}
}
