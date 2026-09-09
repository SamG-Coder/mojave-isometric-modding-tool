// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include <algorithm>
#include <cmath>
namespace damage_numbers {
inline float lostHealth(float before,float after){
 if(!std::isfinite(before)||!std::isfinite(after))return 0;
 return std::max(0.f,std::max(0.f,before)-std::max(0.f,after));
}
// Native DoHealthDamage is notified after the signed health delta is applied.
inline float notifiedLoss(float delta,float healthAfter){
 if(!std::isfinite(delta)||!std::isfinite(healthAfter)||delta>=0)return 0;
 return lostHealth(healthAfter-delta,healthAfter);
}
inline float opacity(float age){return std::clamp((1.4f-age)/.5f,0.f,1.f);}
}
