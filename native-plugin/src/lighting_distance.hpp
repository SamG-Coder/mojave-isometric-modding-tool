// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#pragma once
#include <algorithm>
#include <cmath>
namespace lighting {
inline float visibleDistance(float cameraDistance,float span,float aspect,float pitchDegrees){
 if(!std::isfinite(cameraDistance)||!std::isfinite(span)||!std::isfinite(aspect)||!std::isfinite(pitchDegrees)||cameraDistance<0||span<=0||aspect<=0)return 0;
 float halfWidth=span*.5f;
 float halfGroundDepth=halfWidth/aspect/std::sin(std::clamp(pitchDegrees,20.f,80.f)*.01745329252f);
 // Triangle inequality bounds the distance from the camera to every corner
 // of the visible ground footprint; margin covers actor height and motion.
 return cameraDistance+std::hypot(halfWidth,halfGroundDepth)+256.f;
}
struct FadeOverride {
 bool held{};float baseStart{},baseEnd{},lastStart{},lastEnd{};
 void update(float& start,float& end,bool enabled,float coverage){
  bool ours=held&&start==lastStart&&end==lastEnd;
  if(!enabled||coverage<=0){if(ours){start=baseStart;end=baseEnd;}held=false;return;}
  if(!ours){baseStart=start;baseEnd=end;}
  // Never compound our own override or reduce the user's configured distance.
  start=std::max(baseStart,coverage);end=start+std::max(1.f,baseEnd-baseStart);
  lastStart=start;lastEnd=end;held=true;
 }
};
}
