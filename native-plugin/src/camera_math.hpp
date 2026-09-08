// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include <cmath>
namespace engine {
struct Vec { float x{},y{},z{}; Vec operator+(Vec b) const{return {x+b.x,y+b.y,z+b.z};} Vec operator-(Vec b) const{return {x-b.x,y-b.y,z-b.z};} Vec operator*(float v) const{return {x*v,y*v,z*v};} };
inline float length(Vec v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
inline Vec normalized(Vec v){float n=length(v);return n>0?v*(1/n):Vec{};}
struct Mat { float m[3][3]; };
struct Frustum {float left,right,top,bottom,nearPlane,farPlane; bool ortho; char pad[3];};
static_assert(sizeof(Frustum)==0x1c);
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}

struct CameraSample {
    Vec position{},direction{},up{},right{};Frustum frustum{};
    float x{},y{},width{},height{};bool valid{};
    bool ray(float px,float py,Vec& origin,Vec& directionOut) const {
        if(!valid||width<=0||height<=0||px<x||py<y||px>=x+width||py>=y+height)return false;
        float u=(px-x)/width,v=(py-y)/height;
        float a=frustum.left+u*(frustum.right-frustum.left),b=frustum.top+v*(frustum.bottom-frustum.top);
        origin=position;directionOut=direction;
        if(frustum.ortho)origin=origin+right*a+up*b;
        else directionOut=normalized(direction+right*a+up*b);
        return true;
    }
    bool project(Vec point,float& px,float& py) const {
        if(!valid||width<=0||height<=0)return false;
        Vec relative=point-position;float depth=dot(relative,direction);
        if(depth<=frustum.nearPlane)return false;
        float a=dot(relative,right),b=dot(relative,up);
        if(!frustum.ortho){a/=depth;b/=depth;}
        float fw=frustum.right-frustum.left,fh=frustum.top-frustum.bottom;if(fw<=0||fh<=0)return false;
        px=x+(a-frustum.left)/fw*width;py=y+(frustum.top-b)/fh*height;
        return std::isfinite(px)&&std::isfinite(py)&&px>=x&&py>=y&&px<x+width&&py<y+height;
    }
};
}
