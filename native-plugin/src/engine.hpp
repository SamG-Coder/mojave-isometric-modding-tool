// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder (original contributions).
// See THIRD_PARTY_NOTICES.md for upstream attribution.
#pragma once
#include "context_menu_model.hpp"
#include <windows.h>
#include <cstdint>
#include <cmath>
#include "camera_math.hpp"
// FalloutNV.exe 1.4.0.525 only. Reverse-engineered layout references:
// jazzisparis/JIP-LN-NVSE internal/netimmerse.h, jip_core.cpp, GameOSDepend.h;
// carxt/JohnnyGuitarNVSE CameraOverride.cpp and ActorMover declarations.
namespace engine {
template<class T> T& at(void* p, size_t offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(p)+offset); }
inline void* global(uintptr_t address) { return *reinterpret_cast<void**>(address); }
inline void* player() { return global(0x11DEA3C); }
inline void* cameraChild(void* node){void** children=at<void**>(node,0xA0);return children?children[0]:nullptr;}
inline bool gameMode(){void* ui=global(0x11D8A80);return ui&&at<uint32_t>(ui,0xC)==1;}
inline void movement(uint16_t flags){
    void* p=player();if(!p)return;void* mover=at<void*>(p,0x190);if(!mover)return;
    auto vt=*reinterpret_cast<void***>(mover);
    if(flags)reinterpret_cast<void(__thiscall*)(void*,uint16_t)>(vt[3])(mover,flags);
    else reinterpret_cast<void(__thiscall*)(void*,uint32_t)>(vt[2])(mover,0xFu); // Clear directional flags only.
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
inline uint32_t pipboyMode(){auto ui=global(0x11D8A80);return ui?at<uint32_t>(ui,0x4BC):0;}
inline bool dialogue(){return reinterpret_cast<bool*>(0x11F308F)[1009];}
// Resolve collision geometry through BSFadeNode to its owning reference (JIP adapter).
inline void* parentReference(void* node){
    for(int i=0;node&&i<128;++i,node=at<void*>(node,0x18))
        if(at<uintptr_t>(node,0)==0x10A8F90){auto ref=at<void*>(node,0xCC);if(ref)return ref;}
    return nullptr;
}
// JIP's runtime form-table lookup. 0x4F9620 is NOT this function in 1.4.0.525.
inline void* reference(uint32_t id){
    if(!id)return nullptr;auto table=global(0x11C54C0);if(!table)return nullptr;
    auto count=at<uint32_t>(table,4);auto buckets=at<void**>(table,8);if(!count||count>1000000||!buckets)return nullptr;
    auto entry=buckets[id%count];for(unsigned n=0;entry&&n<100000;n++,entry=at<void*>(entry,0))if(at<uint32_t>(entry,4)==id)return at<void*>(entry,8);
    return nullptr;
}
inline bool interactable(void* ref){
    if(!ref||ref==player())return false;
    if(!context_menu::availableReference(at<uint8_t>(ref,4),at<uint32_t>(ref,8)))return false;
    auto base=at<void*>(ref,0x20);if(!base)return false;
    auto kind=at<uint8_t>(base,4);
    return context_menu::activatable(kind);
}
inline bool areaPickup(void* ref){
    if(!interactable(ref))return false;
    auto base=at<void*>(ref,0x20);auto kind=at<uint8_t>(base,4);
    // Vanilla edible plants use scripted ACTI records, not FLOR records.
    // Preserve their original activation scripts, including quest collection.
    bool harvest=false;
    if(kind==0x15){auto prompt=at<const char*>(base,0x88);auto size=at<uint16_t>(base,0x8C);
        harvest=prompt&&size<512&&size>5&&(_strnicmp(prompt,"Pick ",5)==0||_strnicmp(prompt,"Harvest ",8)==0);
    }
    return context_menu::pickup(kind)||kind==0x26||harvest;
}
inline bool activate(void* ref){return reinterpret_cast<bool(__thiscall*)(void*,void*,uint32_t,uint32_t,uint32_t)>(0x573170)(ref,player(),0,0,1);}

}
