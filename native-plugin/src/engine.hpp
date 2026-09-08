// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder (original contributions).
// See THIRD_PARTY_NOTICES.md for upstream attribution.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cmath>
// FalloutNV.exe 1.4.0.525 only. Reverse-engineered layout references:
// jazzisparis/JIP-LN-NVSE internal/netimmerse.h, jip_core.cpp, GameOSDepend.h;
// carxt/JohnnyGuitarNVSE CameraOverride.cpp and ActorMover declarations.
namespace engine {
template<class T> T& at(void* p, size_t offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(p)+offset); }
inline void* global(uintptr_t address) { return *reinterpret_cast<void**>(address); }
inline void* player() { return global(0x11DEA3C); }
struct Vec { float x{},y{},z{}; Vec operator+(Vec b) const{return {x+b.x,y+b.y,z+b.z};} Vec operator-(Vec b) const{return {x-b.x,y-b.y,z-b.z};} Vec operator*(float v) const{return {x*v,y*v,z*v};} };
inline float length(Vec v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
inline Vec normalized(Vec v){float n=length(v);return n>0?v*(1/n):Vec{};}
struct Mat { float m[3][3]; };
struct Frustum {float left,right,top,bottom,nearPlane,farPlane; bool ortho; char pad[3];};
static_assert(sizeof(Frustum)==0x1c);
inline void* cameraChild(void* node){void** children=at<void**>(node,0xA0);return children?children[0]:nullptr;}
inline bool gameMode(){void* ui=global(0x11D8A80);return ui&&at<uint32_t>(ui,0xC)==1;}
inline void movement(uint16_t flags){
    void* p=player();if(!p)return;void* mover=at<void*>(p,0x190);if(!mover)return;
    auto vt=*reinterpret_cast<void***>(mover);
    if(flags)reinterpret_cast<void(__thiscall*)(void*,uint16_t)>(vt[3])(mover,flags);
    else reinterpret_cast<void(__thiscall*)(void*)>(vt[4])(mover);
}
// TES::RayCast uses aligned Havok data and returns a NiAVObject, not a form.
// The hit fraction also handles landscape hits where the object can be null.
inline bool raycast(Vec origin,Vec direction,Vec& hit,void*& object){
    void* p=player();void* tes=global(0x11DEA10);if(!p||!tes)return false;
    void* process=at<void*>(p,0x68);if(!process)return false;
    void* controller=at<void*>(process,0x138);if(!controller)return false;
    void* proxy=at<void*>(controller,0x594);if(!proxy)return false;
    void* body=at<void*>(proxy,8);if(!body)return false;
    alignas(16) unsigned char data[0xB0]{};
    constexpr float units=0.14287673f, distance=16000.f;
    at<Vec>(data,0)=origin*units;at<Vec>(data,0x10)=(origin+direction*distance)*units;
    at<float>(data,0x40)=1.f;at<uint32_t>(data,0x44)=0xffffffff;at<uint32_t>(data,0x50)=0xffffffff;
    at<uint32_t>(data,0x24)=(at<uint32_t>(body,0x2c)&0xffff0000u)|6;
    object=reinterpret_cast<void*(__thiscall*)(void*,void*,bool)>(0x458440)(tes,data,true);
    float fraction=at<float>(data,0x40);
    if(!std::isfinite(fraction)||fraction<0||fraction>=0.99999f)return false;
    hit=origin+direction*(distance*fraction);return true;
}
}
