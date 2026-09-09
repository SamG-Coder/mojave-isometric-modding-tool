// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#pragma once
#include "camera_math.hpp"
#include <cstdint>
namespace renderer_bridge {
struct MotionConstants {uint32_t width{},height{},history{},reproject{};float previousX[4]{},previousY[4]{};};
static_assert(sizeof(MotionConstants)==48);
inline MotionConstants motionConstants(unsigned w,unsigned h,bool history,const engine::CameraSample& now,const engine::CameraSample& before){
 MotionConstants out{w,h,history?1u:0u,0};
 if(!history||!now.valid||!before.valid||!now.frustum.ortho||!before.frustum.ortho||now.width<=0||now.height<=0||before.width<=0||before.height<=0)return out;
 auto f=now.frustum,p=before.frustum;float fw=f.right-f.left,fh=f.top-f.bottom,pw=p.right-p.left,ph=p.top-p.bottom;
 if(fw<=0||fh<=0||pw<=0||ph<=0)return out;
 auto dx=now.right*(fw/now.width),dy=now.up*(-fh/now.height),dz=now.direction*(f.farPlane-f.nearPlane);
 auto base=now.position+now.right*(f.left-now.x*fw/now.width)+now.up*(f.top+now.y*fh/now.height)+now.direction*f.nearPlane-before.position;
 float sx=before.width/pw,sy=before.height/ph;
 out.previousX[0]=engine::dot(dx,before.right)*sx;out.previousX[1]=engine::dot(dy,before.right)*sx;out.previousX[2]=engine::dot(dz,before.right)*sx;out.previousX[3]=(engine::dot(base,before.right)-p.left)*sx+before.x;
 out.previousY[0]=-engine::dot(dx,before.up)*sy;out.previousY[1]=-engine::dot(dy,before.up)*sy;out.previousY[2]=-engine::dot(dz,before.up)*sy;out.previousY[3]=(p.top-engine::dot(base,before.up))*sy+before.y;
 for(float x:out.previousX)if(!std::isfinite(x))return MotionConstants{w,h,history?1u:0u,0};
 for(float y:out.previousY)if(!std::isfinite(y))return MotionConstants{w,h,history?1u:0u,0};
 out.reproject=1;return out;
}
}
