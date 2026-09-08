// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder (original contributions).
// See THIRD_PARTY_NOTICES.md for upstream attribution.
#include <windows.h>
#include <d3d9.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include "nvse_abi.hpp"
#include "engine.hpp"
#include "wheel_input.hpp"
#include "pathfinder.hpp"
#include "overlay.hpp"
#include "cutaway.hpp"
#include "combat_math.hpp"
#include "route_follower.hpp"
#include "native_navigation.hpp"
using namespace engine;
namespace {
struct AttackOrder {uint32_t id{};Vec point{};void* cell{};bool ordered{},single{},held{};uint64_t started{},lastFire{},lastRepath{},lastReady{};unsigned approach{};} attack;
bool altAiming{},lastVats{};float priorAimPitch{};bool aimOwned{};uint64_t attackRequests{};
void cancelCombat();void combatTick();void attackClick(float,float);
navigation::Search route;navigation::Follower follower;uint64_t routeReuses{},routeSwaps{};unsigned stuckRepairs{};size_t waypoint{};uint64_t routeStarted{},lastPlanningMs{};double plannerCpuMs{},lastPlannerStepMs{};unsigned plannerSteps{};size_t nativeTriangles{};unsigned nativeExpanded{};double nativePlanningMs{};std::string navigationSource="grid";
struct NearbyAction{uint32_t id;Vec pos;std::string text;float range;};std::vector<NearbyAction> nearby;
struct ActionBox{uint32_t id;float x,y,w,h;};std::vector<ActionBox> actionBoxes;
struct MapCell{Vec p;int state=0;};std::array<MapCell,1024> mapCells{};Vec mapOrigin{};size_t mapIndex{};bool mapReady{};
struct Cover{uint32_t id;void* node;};std::vector<Cover> occludingCover;
overlay::Painter hudPainter,fadePainter;float fadeAlpha{};uint64_t fadeHoldUntil{};bool pendingDisable{};
WheelInput wheelInput;std::atomic<int> mouseDeltaX{},mouseDeltaY{};
CameraSample renderCamera{},displayCamera{};float lastPickError=-1;
std::string root, bridge; Console* console{}; Scripts* scripts{};
bool enabled{},requested{},hooksReady{},moving{},captureRequested{},lastL{},lastF{},lastR{};
bool autoEnable=true; uint64_t autoReadySince{};
bool controlsAcquired{},lastMiddle{},pendingActivation{};
uint32_t interactionId{},hoverRef{};uint64_t markerUntil{},activationStarted{},lastFrameTick{};
float cameraBlend=0, frameDt=0.016f, desiredYaw=45,desiredPitch=50;
bool priorThird{}; std::array<bool,3> ownedControls{};
int heldForward=-1;
constexpr int controls[]{4,6,13};
float yaw=45,pitch=50,distance=1100,span=1500,cursorX=640,cursorY=360;
bool orthographic=true; float width=1280,height=720;
Vec cameraPos{},forward{},right{},up{},target{},lastPos{}; Mat rotation{};
void* camera{}; void* savedCamera{}; Frustum savedFrustum{};
Frustum renderedFrustum{}; bool rendererHookReady{},haveRenderedFrustum{};uint64_t projectionUpdates{};
uint64_t frames{},cameraUpdates{},lastSequence{},lastPoll{},lastStatus{},moveStarted{},lastProgress{};
std::string note="Plugin loaded; camera disabled",lastError;
std::map<std::string,void*> expressions;
void log(const std::string& s){std::ofstream(root+"/runtime/native.log",std::ios::app)<<s<<"\n";}
void run(const std::string& line){if(console&&!console->RunScriptLine2(line.c_str(),nullptr,true))log("Console rejected: "+line);}
double number(const std::string& text){
    if(!scripts)return 0;auto& code=expressions[text];if(!code)code=scripts->CompileExpression(text.c_str());
    ScriptResult out{};if(code&&scripts->CallFunction(code,player(),nullptr,&out,0)&&out.type==1)return out.number;return 0;
}
void stopMovement(){follower.clear();route.cancel();waypoint=0;interactionId=0;pendingActivation=false;if(moving)movement(0);if(heldForward>=0){run("ReleaseKey "+std::to_string(heldForward));heldForward=-1;}moving=false;}
void cancelCombat(){
    if(attack.held){run("ReleaseControl 4");attack.held=false;}
    attack.ordered=false;altAiming=false;
    if(aimOwned&&player()){at<float>(player(),0x24)=priorAimPitch;aimOwned=false;}
}
void stop(){cancelCombat();stopMovement();}
void restoreProjection(){if(savedCamera){at<Frustum>(savedCamera,0xDC)=savedFrustum;savedCamera=nullptr;}}
void ownControls(bool acquire){
    if(acquire==controlsAcquired)return;
    for(int i=0;i<3;i++){
        if(acquire){ownedControls[i]=number("IsControlDisabled "+std::to_string(controls[i]))==0;if(ownedControls[i])run("DisableControl "+std::to_string(controls[i]));}
        else if(ownedControls[i]){run("EnableControl "+std::to_string(controls[i]));ownedControls[i]=false;}
    }
    if(acquire&&ownedControls[0]){
        // DisableControl blocks physical AND scripted input. Keep physical attack
        // blocked, but allow mapped native attack requests from this controller.
        auto inputState=global(0x11F35CC);if(inputState){
            auto key=at<uint8_t>(inputState,0x1B94+4),button=at<uint8_t>(inputState,0x1BB0+4);
            if(key!=255)run("EnableKey "+std::to_string(key)+" 2");
            if(button!=255)run("EnableKey "+std::to_string(256+button)+" 2");
        }
    }
    controlsAcquired=acquire;
}
void updateReticle(bool hide);
void restoreCover();
void setEnabled(bool value){
    if(value==enabled)return;
    wheelInput.reset();mouseDeltaX=0;mouseDeltaY=0;
    if(value&&(!hooksReady||!player()||!gameMode())){note="Load a game before enabling the camera";return;}
    if(!value){restoreCover();updateReticle(false);displayCamera.valid=renderCamera.valid=false;stop();restoreProjection();enabled=false;ownControls(false);cameraBlend=0;if(priorThird==false&&player())reinterpret_cast<bool(__thiscall*)(void*,bool)>(0x950110)(player(),true);note="Normal controls restored";return;}
    priorThird=at<uint8_t>(player(),0x64C)!=0;if(!priorThird)reinterpret_cast<bool(__thiscall*)(void*,bool)>(0x950110)(player(),false);
    ownControls(true);
    fadeAlpha=1;fadeHoldUntil=GetTickCount64()+80;enabled=true;cursorX=width/2;cursorY=height/2;note="Isometric prototype enabled";
}
bool active(){return enabled&&gameMode()&&!dialogue()&&player()&&at<uint8_t>(player(),0x64C)&&!pendingActivation&&!pendingDisable;}
// Filter the mouse device result before vanilla camera code sees lZ.
// Chain the existing GetDeviceState implementation, including xNVSE's wrapper.
using GetMouseState=HRESULT(WINAPI*)(void*,DWORD,void*);
std::map<void**,GetMouseState> mouseStateOriginals;
HRESULT WINAPI mouseStateHook(void* device,DWORD bytes,void* buffer){
    auto table=*reinterpret_cast<void***>(device);
    auto found=mouseStateOriginals.find(table);if(found==mouseStateOriginals.end())return E_FAIL;
    HRESULT result=found->second(device,bytes,buffer);
    void* inputState=global(0x11F35CC);
    if(SUCCEEDED(result)&&buffer&&(bytes==16||bytes==20)&&inputState&&device==at<void*>(inputState,0x30)){
        bool owns=active();wheelInput.filter(at<long>(buffer,8),owns);
        if(owns){mouseDeltaX.fetch_add(at<long>(buffer,0));mouseDeltaY.fetch_add(at<long>(buffer,4));at<long>(buffer,0)=at<long>(buffer,4)=0;}
        else {mouseDeltaX=0;mouseDeltaY=0;}

    }
    return result;
}
void installMouseHook(){
    void* inputState=global(0x11F35CC);if(!inputState)return;
    auto device=at<void*>(inputState,0x30);if(!device)return;
    auto table=*reinterpret_cast<void***>(device);if(mouseStateOriginals.count(table))return;
    DWORD old{};if(!VirtualProtect(table+9,sizeof(void*),PAGE_READWRITE,&old))return;
    mouseStateOriginals.emplace(table,reinterpret_cast<GetMouseState>(table[9]));
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(table+9),reinterpret_cast<void*>(mouseStateHook));
    DWORD ignored{};VirtualProtect(table+9,sizeof(void*),old,&ignored);
    log("Mouse wheel isolation hook installed");
}
Frustum worldFrustum(Frustum f){
    f.ortho=true;f.left=-span*.5f;f.right=span*.5f;
    f.top=span*height/width*.5f;f.bottom=-f.top;f.nearPlane=5;
    f.farPlane=std::max(f.farPlane,24000.f);return f;
}
using SetupCamera=void(__thiscall*)(void*,Vec*,Vec*,Vec*,Vec*,Frustum*,float*);
SetupCamera originalSetupCamera{};
void __fastcall setupCameraHook(void* renderer,void*,Vec* pos,Vec* dir,Vec* camUp,Vec* camRight,Frustum* frustum,float* viewport){
    // Scope to the actual world camera; menu, shadow and reflection passes keep their own projection.
    if(enabled&&cameraBlend>0&&camera&&pos&&frustum&&length(*pos-cameraPos)<1.f){
        Frustum custom=*frustum;
        if(orthographic&&cameraBlend>0.999f){custom=worldFrustum(custom);}
        renderedFrustum=custom;haveRenderedFrustum=true;++projectionUpdates;
        originalSetupCamera(renderer,pos,dir,camUp,camRight,&custom,viewport);
        auto device=at<IDirect3DDevice9*>(renderer,0x288);D3DVIEWPORT9 vp{};
        if(dir&&camUp&&camRight&&device&&SUCCEEDED(device->GetViewport(&vp))){
            renderCamera={*pos,*dir,*camUp,*camRight,custom,float(vp.X),float(vp.Y),float(vp.Width),float(vp.Height),true};
        }
    }else originalSetupCamera(renderer,pos,dir,camUp,camRight,frustum,viewport);
}
void basis(){
    constexpr float rad=0.017453292519943295f;float y=yaw*rad,p=pitch*rad;
    forward={sinf(y)*cosf(p),cosf(y)*cosf(p),-sinf(p)};
    right={cosf(y),-sinf(y),0};up={sinf(y)*sinf(p),cosf(y)*sinf(p),cosf(p)};
    rotation={{{right.x,forward.x,up.x},{right.y,forward.y,up.y},{right.z,forward.z,up.z}}};
    // Keep the bottom of the orthographic ground footprint in front of the near plane.
    float safeDistance=orthographic?std::max(distance,span*height/width*.5f/std::tan(p)+500.f):distance;
    cameraPos=at<Vec>(player(),0x30)+Vec{0,0,65}-forward*safeDistance;
}
using SetVector=void(__thiscall*)(void*,const Vec*);
using SetMatrix=void(__thiscall*)(void*,const Mat*);
SetVector originalPos{}; SetMatrix originalRot{};
void __fastcall positionHook(void* node,void*,const Vec* pos){
    if(enabled&&player()&&cameraBlend>0){basis();cameraPos=*pos*(1-cameraBlend)+cameraPos*cameraBlend;originalPos(node,&cameraPos);++cameraUpdates;}else originalPos(node,pos);
}
void __fastcall rotationHook(void* node,void*,const Mat* rot){
    if(!enabled||cameraBlend<=0){restoreProjection();originalRot(node,rot);return;}
    Vec nativeForward{rot->m[0][1],rot->m[1][1],rot->m[2][1]},nativeUp{rot->m[0][2],rot->m[1][2],rot->m[2][2]};
    forward=normalized(nativeForward*(1-cameraBlend)+forward*cameraBlend);
    up=normalized(nativeUp*(1-cameraBlend)+up*cameraBlend);
    right=normalized(cross(forward,up));up=normalized(cross(right,forward));
    rotation={{{right.x,forward.x,up.x},{right.y,forward.y,up.y},{right.z,forward.z,up.z}}};
    originalRot(node,&rotation);camera=cameraChild(node);
    if(camera){
        if(savedCamera!=camera){savedCamera=camera;savedFrustum=at<Frustum>(camera,0xDC);}
        auto& f=at<Frustum>(camera,0xDC);
        if(orthographic&&cameraBlend>0.999f){f=worldFrustum(savedFrustum);}
        else f=savedFrustum;
    }
}
uintptr_t callTarget(uintptr_t a){if(*reinterpret_cast<uint8_t*>(a)!=0xe8)return 0;return a+5+*reinterpret_cast<int32_t*>(a+1);}
bool patchCall(uintptr_t address,void* hookFunction){
    DWORD old{};if(!VirtualProtect(reinterpret_cast<void*>(address),5,PAGE_EXECUTE_READWRITE,&old))return false;
    *reinterpret_cast<int32_t*>(address+1)=int32_t(reinterpret_cast<uintptr_t>(hookFunction)-address-5);
    DWORD ignored;VirtualProtect(reinterpret_cast<void*>(address),5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),5);return true;
}
// The scene culler consumes cached planes before the renderer receives SetupCamera.
// Replace the world-camera volume at the visibility test, including the compound
// view frustum used by Bethesda's culler. Keep reflection/shadow cameras untouched.
using CullObject=void(__thiscall*)(void*,void*);
CullObject originalCull{};uint64_t cullingUpdates{};
void __fastcall cullObjectHook(void* process,void*,void* object){
    if(!active()||!orthographic||cameraBlend<.999f||at<void*>(process,0xC)!=camera){originalCull(process,object);return;}
    const auto oldFrustum=at<Frustum>(process,0x10);
    const auto oldPlanes=at<CullingPlanes>(process,0x2C);
    void* compound=at<void*>(process,0xC0);
    auto f=worldFrustum(at<Frustum>(camera,0xDC));
    at<Frustum>(process,0x10)=f;
    // NiCamera's final world transform uses direction/up/right columns, unlike
    // the parent camera rig. Let the engine construct its own orthographic
    // planes from that transform; never substitute the rig's basis vectors.
    reinterpret_cast<void(__thiscall*)(void*,const Frustum*,const void*)>(0xA74E10)(
        static_cast<char*>(process)+0x2C,&f,static_cast<char*>(camera)+0x68);
    // Compound occlusion/portal volumes are perspective-derived. Reusing them
    // can reject a sphere that intersects the orthographic view. Use the normal
    // six-plane, world-bound sphere path for this camera. This does NOT bypass
    // frustum culling, alter object visibility flags or touch other cameras.
    at<void*>(process,0xC0)=nullptr;
    ++cullingUpdates;originalCull(process,object);
    at<void*>(process,0xC0)=compound;
    at<Frustum>(process,0x10)=oldFrustum;at<CullingPlanes>(process,0x2C)=oldPlanes;
}
bool replaceSlot(uintptr_t slot,void* replacement,void*& original){
    auto current=*reinterpret_cast<uintptr_t*>(slot);if(current<0x400000||current>0x1000000)return false;
    DWORD old{};if(!VirtualProtect(reinterpret_cast<void*>(slot),4,PAGE_READWRITE,&old))return false;
    original=reinterpret_cast<void*>(current);*reinterpret_cast<void**>(slot)=replacement;
    DWORD ignored{};VirtualProtect(reinterpret_cast<void*>(slot),4,old,&ignored);return true;
}
using DrawGeometry=void(__thiscall*)(void*,void*);
std::array<DrawGeometry,6> originalGeometry{};
thread_local bool drawingCover{};uint64_t geometryVisits{},cutawayDraws{};
template<size_t I> void __fastcall geometryHook(void* renderer,void*,void* geometry){
    bool previous=drawingCover;drawingCover=false;++geometryVisits;
    if(active()&&!occludingCover.empty()&&geometry){auto ref=parentReference(geometry);
        if(ref)for(auto cover:occludingCover)if(at<uint32_t>(ref,0xC)==cover.id){drawingCover=true;break;}
    }
    originalGeometry[I](renderer,geometry);drawingCover=previous;
}
using DrawIndexed=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
DrawIndexed originalDrawIndexed{};
HRESULT WINAPI drawIndexedHook(IDirect3DDevice9* device,D3DPRIMITIVETYPE type,INT base,UINT min,UINT vertices,UINT start,UINT count){
    cutaway::Mask mask;bool applied=false;
    // Screen-space projection is taken from this render pass, never last frame.
    if(drawingCover&&renderCamera.valid&&player()&&length(renderCamera.position-cameraPos)<1){
        float x{},y{};if(renderCamera.project(at<Vec>(player(),0x30)+Vec{0,0,65},x,y))applied=mask.begin(device,x,y,std::clamp(height*.15f,70.f,160.f));
    }
    HRESULT hr=originalDrawIndexed(device,type,base,min,vertices,start,count);
    if(applied){++cutawayDraws;mask.end();}return hr;
}
void installCutawayDevice(IDirect3DDevice9* device){
    if(originalDrawIndexed)return;auto table=*reinterpret_cast<void***>(device);DWORD old{};
    if(!VirtualProtect(table+82,sizeof(void*),PAGE_READWRITE,&old))return;
    originalDrawIndexed=reinterpret_cast<DrawIndexed>(table[82]);table[82]=reinterpret_cast<void*>(drawIndexedHook);
    DWORD ignored{};VirtualProtect(table+82,sizeof(void*),old,&ignored);log("Local cutaway draw hook installed");
}
void installHooks(){
    constexpr uintptr_t p1=0x94AD8A,p2=0x94BDC2,r1=0x94AD9D,r2=0x94BDD5;
    uintptr_t p=callTarget(p1),r=callTarget(r1);
    if(p<0x400000||p>0x1000000||r<0x400000||r>0x1000000||p!=callTarget(p2)||r!=callTarget(r2)){
        lastError="Camera call sites differ from supported 1.4.0.525; hooks refused";log(lastError);return;
    }
    originalPos=reinterpret_cast<SetVector>(p);originalRot=reinterpret_cast<SetMatrix>(r);
    // Validate every site before modifying any. Matching callers also avoids silently stacking camera mods.
    hooksReady=patchCall(p1,reinterpret_cast<void*>(positionHook))&&patchCall(p2,reinterpret_cast<void*>(positionHook))&&patchCall(r1,reinterpret_cast<void*>(rotationHook))&&patchCall(r2,reinterpret_cast<void*>(rotationHook));
    log(hooksReady?"Native camera hooks installed":"Camera hook installation failed");
    void* previousCull{};
    if(replaceSlot(0x101E2EC+0x44,reinterpret_cast<void*>(cullObjectHook),previousCull)){
        originalCull=reinterpret_cast<CullObject>(previousCull);log("World-transform orthographic bound culling installed (compound occlusion excluded)");
    }else {lastError="World culling hook unavailable";log(lastError);}
    constexpr uintptr_t rendererSlot=0x10EE4BC+0x18C;
    uintptr_t setup=*reinterpret_cast<uintptr_t*>(rendererSlot);
    if(setup>0x400000&&setup<0x1000000){
        DWORD old{};if(VirtualProtect(reinterpret_cast<void*>(rendererSlot),4,PAGE_READWRITE,&old)){
            originalSetupCamera=reinterpret_cast<SetupCamera>(setup);
            *reinterpret_cast<void**>(rendererSlot)=reinterpret_cast<void*>(setupCameraHook);
            DWORD ignored;VirtualProtect(reinterpret_cast<void*>(rendererSlot),4,old,&ignored);rendererHookReady=true;log("Native renderer SetupCamera hook installed");
        }
    }
}
bool pick(float x,float y,Vec& hit,void*& hitObject){
    if(!active()||!camera||cameraBlend<0.999f)return false;
    Vec origin{},ray{};
    if(!displayCamera.ray(x,y,origin,ray))return false;
    auto cell=at<void*>(player(),0x40);
    // Interior destination selection uses the player's current floor. Clip the
    // same screen ray below roofs, rather than changing its screen alignment.
    // Exterior roofs are ignored only when a ceiling is detected above the player.
    Vec feet=at<Vec>(player(),0x30),ceiling{};void* ceilingObject{};
    bool indoors=cell&&(at<uint8_t>(cell,0x24)&1);
    bool covered=indoors;
    if(!covered)covered=raycast(feet+Vec{0,0,110},{0,0,1},ceiling,ceilingObject)&&ceiling.z-feet.z<600;
    if(covered)origin=rayBelowHeight(origin,ray,feet.z+100);
    return raycast(origin,ray,hit,hitObject);
}
bool groundProbe(Vec guess,Vec& ground){
    void* object{};if(!raycast(guess+Vec{0,0,44},{0,0,-1},ground,object))return false;
    if(std::abs(ground.z-guess.z)>64)return false;
    Vec ceiling{};if(raycast(ground+Vec{0,0,5},{0,0,1},ceiling,object)&&ceiling.z-ground.z<110)return false;
    return true;
}
bool corridorClear(Vec a,Vec b){
    Vec delta=b-a;float len=length(delta);if(len<1)return true;
    if(std::abs(delta.z)>std::hypot(delta.x,delta.y)*.8f+18)return false;
    Vec side=normalized(Vec{-delta.y,delta.x,0})*19;
    // Ensure longer segments are supported: body rays alone can cross gaps.
    if(len>40)for(float t=32;t<len;t+=32){Vec floor{};if(!groundProbe(a+delta*(t/len),floor)||std::abs(floor.z-(a+delta*(t/len)).z)>40)return false;}
    for(int lane=-1;lane<=1;lane++)for(float bodyHeight:{28.f,95.f}){
        Vec origin=a+side*float(lane)+Vec{0,0,bodyHeight},hit{};void* object{};
        if(raycast(origin,delta*(1/len),hit,object)&&length(hit-origin)<len-3)return false;
    }
    return true;
}
void pauseWalking(){
    if(moving)movement(0);if(heldForward>=0){run("ReleaseKey "+std::to_string(heldForward));heldForward=-1;}moving=false;
}
void resumeWalking(){
    if(moving)return;
    auto inputState=global(0x11F35CC);heldForward=inputState?at<uint8_t>(inputState,0x1B94):-1;
    if(heldForward<0||heldForward>=255){heldForward=-1;note="Bind forward movement to a keyboard key";return;}
    run("HoldKey "+std::to_string(heldForward));moving=true;moveStarted=lastProgress=GetTickCount64();lastPos=at<Vec>(player(),0x30);
}
bool tryNativeRoute(Vec pos){
    std::vector<Vec> path;auto started=std::chrono::steady_clock::now();
    bool found=native_navigation::plan(pos,target,interactionId?150.f:60.f,path,nativeTriangles,nativeExpanded);
    nativePlanningMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    if(!found||!follower.adopt(path,pos,corridorClear))return false;
    route.cancel();navigationSource="navmesh";++routeSwaps;lastPlanningMs=uint64_t(nativePlanningMs);resumeWalking();note="Following native navigation mesh";return true;
}
void planDestination(Vec hit,uint32_t id){
    stuckRepairs=0;moveStarted=GetTickCount64();target=hit;interactionId=id;pendingActivation=false;markerUntil=GetTickCount64()+3000;
    auto pos=at<Vec>(player(),0x30);
    // Small adjustments can splice into the validated route without an A* restart.
    if(!id&&follower.adjust(pos,hit,corridorClear)){
        route.cancel();navigationSource="route_reuse";++routeReuses;resumeWalking();note="Adjusted existing route";return;
    }
    if(tryNativeRoute(pos))return;
    navigationSource="grid_fallback";
    route.begin(pos,target,id?100.f:20.f);routeStarted=GetTickCount64();plannerCpuMs=0;plannerSteps=0;
    note=moving?"Updating route while walking":"Planning route";
}
void nearbyDestination(uint32_t id){cancelCombat();auto ref=reference(id);if(!interactable(ref))return;planDestination(at<Vec>(ref,0x30),id);}
void beginWalking(){
    lastPlanningMs=GetTickCount64()-routeStarted;auto pos=at<Vec>(player(),0x30);
    if(!follower.adopt(route.path,pos,corridorClear)){
        // The player may have advanced while the search ran. Rebase the pending
        // search without destroying the active path or releasing movement.
        navigationSource="grid_repair";route.begin(pos,target,interactionId?100.f:20.f);routeStarted=GetTickCount64();plannerCpuMs=0;plannerSteps=0;return;
    }
    ++routeSwaps;resumeWalking();note=interactionId?"Following route to interaction":"Following updated route";
}
void destination(float x,float y){
    if(!active())return;cancelCombat();
    void* hitObject{};Vec hit{};
    if(!pick(x,y,hit,hitObject)){note="No collision surface under cursor";stop();return;}
    Vec pos=at<Vec>(player(),0x30);if(length(hit-pos)>6000){note="Destination too far away";stop();return;}
    void* ref=parentReference(hitObject);uint32_t pickedId=interactable(ref)?at<uint32_t>(ref,0xC):0;
    // A height difference alone says nothing about whether a slope is walkable.
    // Probe the surface locally; reject vertical faces and excessively steep ground.
    if(!pickedId){
        Vec ground{};void* surface{};
        if(!raycast(hit+Vec{0,0,12},{0,0,-1},ground,surface)||std::abs(ground.z-hit.z)>20){stop();note="Choose a ground surface, not a wall";return;}
        Vec gx{},gy{};void* other{};
        if(raycast(ground+Vec{20,0,60},{0,0,-1},gx,other)&&raycast(ground+Vec{0,20,60},{0,0,-1},gy,other)){
            float rise=std::sqrt((gx.z-ground.z)*(gx.z-ground.z)+(gy.z-ground.z)*(gy.z-ground.z));
            if(rise>28){stop();note="Surface too steep to walk on";return;}
        }
        // Keep the original ray hit: replacing its Z with the probe shifts the click on slopes.
    }
    float projectedX{},projectedY{};lastPickError=displayCamera.project(hit,projectedX,projectedY)?std::hypot(projectedX-x,projectedY-y):-1;
    planDestination(hit,pickedId);
}
void walk(){
    if(route.state==navigation::Search::State::Searching){
        if(!active()){stopMovement();return;}
        auto planningStart=std::chrono::steady_clock::now();
        route.step(groundProbe,corridorClear,12,2.f);
        lastPlannerStepMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-planningStart).count();
        plannerCpuMs+=lastPlannerStepMs;++plannerSteps;
        if(route.state==navigation::Search::State::Found)beginWalking();
        else if(route.state==navigation::Search::State::NoPath||GetTickCount64()-routeStarted>12000){lastPlanningMs=GetTickCount64()-routeStarted;route.cancel();note="Replacement route unavailable; finishing safe route";}
    }
    if(!moving)return;if(!active()){stopMovement();return;}
    Vec pos=at<Vec>(player(),0x30),delta=target-pos;
    if(interactionId&&(!interactable(reference(interactionId))||at<void*>(reference(interactionId),0x40)!=at<void*>(player(),0x40))){stopMovement();note="Interaction target no longer available";return;}
    float reach=interactionId?110.f:24.f;
    if(std::sqrt(delta.x*delta.x+delta.y*delta.y)<reach&&std::abs(delta.z)<(interactionId?150.f:45.f)){
        uint32_t id=interactionId;stopMovement();markerUntil=GetTickCount64()+900;
        if(id){interactionId=id;pendingActivation=true;activationStarted=GetTickCount64();ownControls(false);note="Approaching interaction camera";}
        else note="Destination reached";
        return;
    }
    auto now=GetTickCount64();if(length(pos-lastPos)>12){lastPos=pos;lastProgress=now;}
    if(now-moveStarted>120000){stopMovement();note="Movement order timed out";return;}
    if(now-lastProgress>1800){
        if(++stuckRepairs>3){stopMovement();note="Unable to recover blocked movement";return;}
        pauseWalking();navigationSource="grid_repair";route.begin(pos,target,interactionId?100.f:20.f);routeStarted=now;plannerCpuMs=0;plannerSteps=0;note="Repairing stalled route";return;
    }
    follower.advance(pos);
    if(follower.done()){
        pauseWalking();note=route.state==navigation::Search::State::Searching?"Waiting at end of safe route":"Route ended";return;
    }
    delta=follower.points[follower.cursor]-pos;
    static uint64_t lastCorridorCheck{},lastLookAhead{};
    if(now-lastCorridorCheck>=100){lastCorridorCheck=now;
        follower.shortcut(pos,corridorClear);delta=follower.points[follower.cursor]-pos;
        if(!corridorClear(pos,pos+normalized(delta)*std::min(35.f,length(delta)))){
            // Stop only when the immediate corridor is physically blocked.
            pauseWalking();
            if(route.state!=navigation::Search::State::Searching){navigationSource="grid_repair";route.begin(pos,target,interactionId?100.f:20.f);routeStarted=now;plannerCpuMs=0;plannerSteps=0;}
            note="Immediate path blocked; finding detour";return;
        }
    }
    // Detect a changed segment before reaching it, while the current segment
    // remains walkable. A replacement search runs alongside the active route.
    if(now-lastLookAhead>=350&&route.state!=navigation::Search::State::Searching){lastLookAhead=now;
        size_t next=follower.cursor+1;
        if(next<follower.points.size()&&!corridorClear(follower.points[follower.cursor],follower.points[next])){
            navigationSource="grid_repair";route.begin(pos,target,interactionId?100.f:20.f);routeStarted=now;plannerCpuMs=0;plannerSteps=0;note="Repairing route ahead while walking";
        }
    }
    at<float>(player(),0x2c)=atan2f(delta.x,delta.y);
    movement(1|512);
}
bool combatActor(void* ref){
    if(!ref||ref==player())return false;auto type=at<uint8_t>(ref,4);
    return (type==0x3B||type==0x3C)&&!(at<uint32_t>(ref,8)&0x820)&&at<uint32_t>(ref,0x108)!=1&&at<uint32_t>(ref,0x108)!=2;
}
void* namedBone(void* node,const char* wanted,unsigned depth=0){
    if(!node||depth>32)return nullptr;
    auto name=at<const char*>(node,8);if(name&&!std::strcmp(name,wanted))return node;
    // NiNode::GetAsNiNode returns this; geometry uses the null implementation.
    auto table=at<uintptr_t*>(node,0);if(table[3]!=0x6815C0)return nullptr;
    auto children=at<void**>(node,0xA0);auto count=at<uint16_t>(node,0xA6);if(!children||count>512)return nullptr;
    for(unsigned i=0;i<count;i++)if(auto found=namedBone(children[i],wanted,depth+1))return found;
    return nullptr;
}
void* actorRoot(void* ref){auto render=ref?at<void*>(ref,0x64):nullptr;return render?at<void*>(render,0x14):nullptr;}
bool validActorPoint(Vec point,Vec feet){return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z)&&length(point-feet)<400;}
Vec bodyPoint(void* ref){
    auto pos=at<Vec>(ref,0x30);auto node=actorRoot(ref);
    // Animated torso position follows crouching and body motion. Scene bounds
    // can include equipment and are only a fallback for non-humanoid rigs.
    if(auto torso=namedBone(node,"Bip01 Spine2")){auto point=at<Vec>(torso,0x8C);if(validActorPoint(point,pos))return point;}
    auto bound=node?at<void*>(node,0x20):nullptr;
    if(bound){auto centre=at<Vec>(bound,0);if(validActorPoint(centre,pos))return centre;}
    return pos+Vec{0,0,65};
}
Vec firingOrigin(){
    auto pos=at<Vec>(player(),0x30);auto process=at<void*>(player(),0x68);
    if(process&&at<uint8_t>(process,0x28)<=1&&at<uint8_t>(process,0x135)){
        auto projectileNode=at<void*>(process,0x130);
        if(projectileNode){auto muzzle=at<Vec>(projectileNode,0x8C);if(validActorPoint(muzzle,pos))return muzzle;}
    }
    if(auto head=namedBone(actorRoot(player()),"Bip01 Head")){auto eye=at<Vec>(head,0x8C);if(validActorPoint(eye,pos))return eye;}
    return pos+Vec{0,0,95};
}
void facePoint(Vec point){
    if(!aimOwned){priorAimPitch=at<float>(player(),0x24);aimOwned=true;}
    auto aim=combat::aim(firingOrigin(),point);
    at<float>(player(),0x2C)=aim.yaw;at<float>(player(),0x24)=std::clamp(aim.pitch,-1.35f,1.35f);
}
bool shotClear(Vec from,Vec to,void* victim){
    auto delta=to-from;auto shotDistance=length(delta);if(shotDistance<1)return true;
    Vec hit{};void* object{};
    if(!raycast(from,delta*(1/shotDistance),hit,object))return true;
    return length(hit-from)>=shotDistance-8||(victim&&parentReference(object)==victim);
}
void releaseAttack(){if(attack.held){run("ReleaseControl 4");attack.held=false;}}
void attackClick(float x,float y){
    Vec point{};void* object{};if(!pick(x,y,point,object))return;
    auto ref=parentReference(object);cancelCombat();stopMovement();
    attack={};attack.id=combatActor(ref)?at<uint32_t>(ref,0xC):0;
    attack.point=attack.id?bodyPoint(ref):point;attack.single=!attack.id;
    attack.cell=at<void*>(player(),0x40);attack.ordered=true;attack.started=GetTickCount64();
    note=attack.id?"Attack target selected":"Single attack at cursor";
}
void combatTick(){
    auto now=GetTickCount64();
    if(!active()||!player()||at<uint32_t>(player(),0x108)==1||at<uint32_t>(player(),0x108)==2){cancelCombat();return;}
    if(!attack.ordered){
        if(altAiming&&!lastMiddle){Vec hit{};void* object{};if(pick(cursorX,cursorY,hit,object)){auto ref=parentReference(object);facePoint(combatActor(ref)?bodyPoint(ref):hit);}}
        else if(aimOwned){at<float>(player(),0x24)=priorAimPitch;aimOwned=false;}
        return;
    }
    auto victim=attack.id?reference(attack.id):nullptr;
    if(attack.cell!=at<void*>(player(),0x40)||(attack.id&&(!combatActor(victim)||at<void*>(victim,0x40)!=attack.cell))){stop();note="Attack target no longer available";return;}
    if(now-attack.started>120000){stop();note="Attack order timed out";return;}
    if(victim)attack.point=bodyPoint(victim);
    auto process=at<void*>(player(),0x68);auto entry=process&&at<uint8_t>(process,0x28)<=1?at<void*>(process,0x114):nullptr;
    auto weapon=entry?at<void*>(entry,8):nullptr;auto type=weapon?at<uint8_t>(weapon,0xF4):0;
    bool melee=type<=2,automatic=weapon&&(at<uint8_t>(weapon,0x100)&2);
    float range=melee?(weapon?at<float>(weapon,0xFC)*100.f:95.f):(weapon?at<float>(weapon,0x124):1200.f);
    if(!std::isfinite(range)||range<=0)range=melee?95.f:1200.f;
    range=std::clamp(range,60.f,5000.f);
    auto pos=at<Vec>(player(),0x30),eye=firingOrigin();float targetDistance=length(attack.point-eye);
    bool clear=shotClear(eye,attack.point,victim);
    if(targetDistance>range||!clear){
        releaseAttack();if(attack.single){stop();note="Shot is blocked or out of range";return;}
        if(now-attack.lastRepath>=1000&&(route.state!=navigation::Search::State::Searching)&&(!moving||length(target-at<Vec>(victim,0x30))>range*1.5f)){
            attack.lastRepath=now;auto centre=at<Vec>(victim,0x30);auto toward=normalized(Vec{pos.x-centre.x,pos.y-centre.y,0});
            float base=std::atan2(toward.y,toward.x);bool planned=false;
            for(unsigned n=0;n<8;n++){float angle=base+float((attack.approach+n)%8)*.78539816f;Vec candidate=centre+Vec{std::cos(angle),std::sin(angle),0}*(range*.7f),floor{};
                if(groundProbe(candidate,floor)&&shotClear(floor+Vec{0,0,95},attack.point,victim)){
                    planDestination(floor,0);attack.approach=(attack.approach+n+1)%8;planned=true;note="Moving to attack position";break;
                }
            }
            if(!planned){stopMovement();note="No clear attack position found";}
        }
        return;
    }
    stopMovement();facePoint(attack.point);
    if(!ownedControls[0]){releaseAttack();note="Attack input owned by another control system";return;}
    if(!process)return;
    if(!at<uint8_t>(process,0x135)){
        releaseAttack();if(now-attack.lastReady>1000){run("TapControl 7");attack.lastReady=now;}note="Readying weapon";return;
    }
    if(!combat::canAttack(targetDistance,range,clear,moving,active(),!victim||combatActor(victim))){releaseAttack();return;}
    // Submit mapped native input, preserving ammunition, reloads and animation gates.
    if(automatic&&!attack.single){if(!attack.held){run("HoldControl 4");attack.held=true;++attackRequests;}}
    else {
        float rate=weapon?at<float>(weapon,0x134):2.f;if(!std::isfinite(rate)||rate<=0)rate=2;
        auto interval=uint64_t(std::clamp(1000.f/rate,100.f,2000.f));
        if(now-attack.lastFire>=interval){run("TapControl 4");attack.lastFire=now;++attackRequests;if(attack.single){attack.ordered=false;}}
    }
    note="Attacking selected target";
}
void status(){
    std::ostringstream s;s<<"{\"pid\":"<<GetCurrentProcessId()<<",\"frames\":"<<frames<<",\"camera_updates\":"<<cameraUpdates<<",\"hooks_ready\":"<<(hooksReady?"true":"false")<<",\"enabled\":"<<(enabled?"true":"false")<<",\"game_mode\":"<<(gameMode()?"true":"false")<<",\"orthographic\":"<<(orthographic?"true":"false")<<",\"moving\":"<<(moving?"true":"false")<<",\"width\":"<<width<<",\"height\":"<<height<<",\"cursor\":["<<cursorX<<","<<cursorY<<"],\"target\":["<<target.x<<","<<target.y<<","<<target.z<<"]";
    s<<",\"camera_blend\":"<<cameraBlend<<",\"yaw\":"<<yaw<<",\"controls_owned\":"<<(controlsAcquired?"true":"false")<<",\"dialogue\":"<<(dialogue()?"true":"false")<<",\"interaction_id\":"<<interactionId<<",\"hover_ref\":"<<hoverRef<<",\"pitch\":"<<pitch<<",\"pick_error_pixels\":"<<lastPickError<<",\"planning\":"<<(route.state==navigation::Search::State::Searching?"true":"false")<<",\"path_nodes\":"<<follower.points.size()<<",\"route_reuses\":"<<routeReuses<<",\"route_swaps\":"<<routeSwaps<<",\"expanded_nodes\":"<<route.expanded<<",\"ground_queries\":"<<route.groundQueries<<",\"edge_queries\":"<<route.edgeQueries<<",\"path_cache_hits\":"<<route.cacheHits<<",\"planning_ms\":"<<lastPlanningMs<<",\"planner_cpu_ms\":"<<plannerCpuMs<<",\"planner_step_ms\":"<<lastPlannerStepMs<<",\"planner_updates\":"<<plannerSteps<<",\"navigation_source\":\""<<navigationSource<<"\",\"native_triangles\":"<<nativeTriangles<<",\"native_expanded\":"<<nativeExpanded<<",\"native_planning_ms\":"<<nativePlanningMs<<",\"attack_target\":"<<attack.id<<",\"attack_ordered\":"<<(attack.ordered?"true":"false")<<",\"attack_requests\":"<<attackRequests<<",\"direct_route\":"<<(route.directRoute?"true":"false")<<",\"nearby_actions\":"<<nearby.size()<<",\"cutaway_occluders\":"<<occludingCover.size()<<",\"fade_alpha\":"<<fadeAlpha;
    if(player()){auto p=at<Vec>(player(),0x30);s<<",\"player\":["<<p.x<<","<<p.y<<","<<p.z<<"]";}
    s<<",\"renderer_hook_ready\":"<<(rendererHookReady?"true":"false")<<",\"projection_updates\":"<<projectionUpdates<<",\"culling_updates\":"<<cullingUpdates<<",\"geometry_visits\":"<<geometryVisits<<",\"cutaway_draws\":"<<cutawayDraws<<",\"rendered_orthographic\":"<<(haveRenderedFrustum&&renderedFrustum.ortho?"true":"false")<<",\"note\":\""<<note<<"\",\"error\":\""<<lastError<<"\"}";
    {std::ofstream f(bridge+"/status.tmp");f<<s.str();}MoveFileExA((bridge+"/status.tmp").c_str(),(bridge+"/status.json").c_str(),MOVEFILE_REPLACE_EXISTING);
}
float setting(const char* name,float fallback){char value[64];GetPrivateProfileStringA("camera",name,std::to_string(fallback).c_str(),value,64,(bridge+"/command.ini").c_str());char* end{};float n=strtof(value,&end);return end!=value&&std::isfinite(n)?n:fallback;}
void poll(){
    const auto now=GetTickCount64();if(now-lastPoll<100)return;lastPoll=now;
    const std::string file=bridge+"/command.ini";
    auto seq=GetPrivateProfileIntA("command","sequence",0,file.c_str());if(seq&&seq!=lastSequence){
        lastSequence=seq;desiredYaw=setting("yaw",desiredYaw);desiredPitch=std::clamp(setting("pitch",desiredPitch),20.f,80.f);distance=std::clamp(setting("distance",distance),200.f,4000.f);span=std::clamp(setting("span",span),300.f,5000.f);
        orthographic=GetPrivateProfileIntA("camera","orthographic",orthographic,file.c_str())!=0;
        char op[40];GetPrivateProfileStringA("command","operation","configure",op,40,file.c_str());
        if(!strcmp(op,"enable")){requested=true;setEnabled(true);}else if(!strcmp(op,"disable")){requested=false;pendingDisable=true;stop();}
        else if(!strcmp(op,"capture"))captureRequested=true;
        else if(!strcmp(op,"stop"))stop();
        else if(!strcmp(op,"inspect")){void* object{};Vec point{};hoverRef=0;if(pick(setting("x",cursorX),setting("y",cursorY),point,object)){auto ref=parentReference(object);hoverRef=ref?at<uint32_t>(ref,0xC):0;}status();}
        else if(!strcmp(op,"interact"))nearbyDestination(uint32_t(setting("id",0)));
        else if(!strcmp(op,"attack"))attackClick(setting("x",cursorX),setting("y",cursorY));
        else if(!strcmp(op,"move"))destination(setting("x",width/2),setting("y",height/2));
        else if(!strcmp(op,"console")){char line[512];GetPrivateProfileStringA("command","text","",line,512,file.c_str());if(*line){run(line);note="Console command submitted; inspect frame for result";}}
    }
    if(now-lastStatus>500){lastStatus=now;status();}
}
void input(){
    DWORD foreground{};GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    if(foreground!=GetCurrentProcessId()){mouseDeltaX=0;mouseDeltaY=0;stop();lastL=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;lastR=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;return;}
    bool f=(GetAsyncKeyState(VK_F8)&0x8000)!=0;if(f&&!lastF){requested=!enabled;if(requested)setEnabled(true);else{pendingDisable=true;stop();}}lastF=f;
    altAiming=(GetAsyncKeyState(VK_MENU)&0x8000)!=0;
    bool vats=(GetAsyncKeyState('V')&0x8000)!=0;if(vats&&!lastVats){stop();note="VATS handoff: live attack cancelled";}lastVats=vats;
    bool l=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0,r=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
    if(!active()){if(!pendingActivation)stop();lastL=l;lastR=r;lastMiddle=false;return;}
    void* in=global(0x11F35CC);if(in){
        bool middle=(GetAsyncKeyState(VK_MBUTTON)&0x8000)!=0;
        int dx=mouseDeltaX.exchange(0),dy=mouseDeltaY.exchange(0);
        if(middle){desiredYaw+=float(dx)*.35f;desiredPitch=std::clamp(desiredPitch+float(dy)*.25f,20.f,80.f);}
        else {cursorX=std::clamp(cursorX+float(dx),0.f,width-1);cursorY=std::clamp(cursorY+float(dy),0.f,height-1);}
        lastMiddle=middle;
        int wheel=wheelInput.take();if(wheel){distance=std::clamp(distance-wheel*.5f,200.f,4000.f);span=std::clamp(span-wheel*.5f,300.f,5000.f);}}
    if(GetAsyncKeyState(VK_OEM_4)&0x8000)desiredYaw-=100.f*frameDt;
    if(GetAsyncKeyState(VK_OEM_6)&0x8000)desiredYaw+=100.f*frameDt;
    if(l&&!lastL&&!lastMiddle&&altAiming){attackClick(cursorX,cursorY);}
    else if(l&&!lastL&&!lastMiddle){bool selected=false;for(auto box:actionBoxes)if(cursorX>=box.x&&cursorY>=box.y&&cursorX<box.x+box.w&&cursorY<box.y+box.h){nearbyDestination(box.id);selected=true;break;}
        if(!selected)destination(cursorX,cursorY);
    }if(r&&!lastR)stop();lastL=l;lastR=r;
    static uint64_t lastHover{};auto now=GetTickCount64();
    if(now-lastHover>100){bool overPrompt=false;for(auto box:actionBoxes)if(cursorX>=box.x&&cursorY>=box.y&&cursorX<box.x+box.w&&cursorY<box.y+box.h)overPrompt=true;lastHover=now;if(!overPrompt){hoverRef=0;Vec hit{};void* object{};if(pick(cursorX,cursorY,hit,object)){auto ref=parentReference(object);if(interactable(ref))hoverRef=at<uint32_t>(ref,0xC);}}}
    if(GetAsyncKeyState(VK_ESCAPE)&0x8000)stop();if(altAiming&&!attack.ordered)stopMovement();walk();combatTick();
}
void capture(IDirect3DDevice9* device){
    IDirect3DSurface9* back{};IDirect3DSurface9* staging{};IDirect3DSurface9* resolved{};
    HRESULT hr=device->GetRenderTarget(0,&back);if(FAILED(hr))return;
    D3DSURFACE_DESC desc{};back->GetDesc(&desc);
    if(desc.MultiSampleType!=D3DMULTISAMPLE_NONE){
        hr=device->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&resolved,nullptr);
        if(SUCCEEDED(hr))hr=device->StretchRect(back,nullptr,resolved,nullptr,D3DTEXF_NONE);
    }
    if(FAILED(hr)){if(resolved)resolved->Release();back->Release();lastError="Could not resolve render target";return;}
    hr=device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&staging,nullptr);
    if(SUCCEEDED(hr))hr=device->GetRenderTargetData(resolved?resolved:back,staging);
    D3DLOCKED_RECT lock{};
    if(SUCCEEDED(hr)&&(desc.Format==D3DFMT_X8R8G8B8||desc.Format==D3DFMT_A8R8G8B8)&&SUCCEEDED(staging->LockRect(&lock,nullptr,D3DLOCK_READONLY))){
        BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+desc.Width*desc.Height*4;
        info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-LONG(desc.Height);info.biPlanes=1;info.biBitCount=32;
        std::ofstream out(bridge+"/frame.tmp",std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&info),sizeof(info));
        for(UINT y=0;y<desc.Height;y++)out.write(static_cast<char*>(lock.pBits)+y*lock.Pitch,desc.Width*4);
        out.close();staging->UnlockRect();MoveFileExA((bridge+"/frame.tmp").c_str(),(bridge+"/frame.bmp").c_str(),MOVEFILE_REPLACE_EXISTING);note="Renderer frame captured";
    }else{lastError="Frame capture failed: disable multisample antialiasing for readback";}
    if(staging)staging->Release();if(resolved)resolved->Release();back->Release();
}
// HUD colour is packed RRGGBBAA; D3D Clear uses AARRGGBB.
DWORD hudColour(){return 0xff000000u|(*reinterpret_cast<uint32_t*>(0x11D8AD4+4)>>8);}
void* tileValue(void* tile,uint32_t id){
    if(!tile)return nullptr;auto values=at<void**>(tile,0x14);auto count=at<uint32_t>(tile,0x18);
    if(!values||count>4096)return nullptr;
    for(uint32_t i=0;i<count;i++)if(values[i]&&at<uint32_t>(values[i],0)==id)return values[i];return nullptr;
}
void updateReticle(bool hide){
    static void* heldTile{};static float previousVisible{};
    auto hud=global(0x11D96C0);auto tile=hud?at<void*>(hud,0xF4):nullptr;
    if(heldTile&&heldTile!=tile)heldTile=nullptr;
    if(hide&&tile){
        if(!heldTile){auto value=tileValue(tile,0xFA3);if(!value)return;previousVisible=at<float>(value,8);heldTile=tile;}
        reinterpret_cast<void(__thiscall*)(void*,uint32_t,float,bool)>(0xA012D0)(tile,0xFA3,0,true);
    }else if(heldTile){reinterpret_cast<void(__thiscall*)(void*,uint32_t,float,bool)>(0xA012D0)(heldTile,0xFA3,previousVisible,true);heldTile=nullptr;}
}
void destinationMarker(IDirect3DDevice9* device){
    if(!moving&&route.state!=navigation::Search::State::Searching&&GetTickCount64()>markerUntil)return;
    float centreX{},centreY{};if(!renderCamera.project(target,centreX,centreY))return;
    std::array<D3DRECT,64> dots{};DWORD count=0;
    for(int i=0;i<64;i++){
        float a=float(i)*6.2831853f/64.f;
        LONG px=LONG(std::lround(centreX+cosf(a)*10)),py=LONG(std::lround(centreY+sinf(a)*7));
        if(px<1||py<1||px>=LONG(width)-2||py>=LONG(height)-2)continue;
        dots[count++]={px-1,py-1,px+2,py+2};
    }
    if(count)device->Clear(count,dots.data(),D3DCLEAR_TARGET,hudColour(),1,0);
}
std::string actionName(void* ref){
    auto base=at<void*>(ref,0x20);auto type=at<uint8_t>(base,4);int offset=(type==0x2A||type==0x2B)?0xD0:(type==0x1B?0x3C:0x30);
    const char* name=at<const char*>(base,offset+4);auto len=at<uint16_t>(base,offset+8);
    std::string label=(name&&len&&len<512)?std::string(name,std::min<size_t>(len,27)):"Object";
    for(char& c:label)if(static_cast<unsigned char>(c)<32)c=' ';
    return (type==0x1C?"Enter ":((type==0x2A||type==0x2B||type==0x16)?"Talk to ":"Use "))+label;
}
void restoreCover(){
    occludingCover.clear();
}
void sampleWorld(){
    if(!active()){restoreCover();nearby.clear();actionBoxes.clear();return;}
    auto p=at<Vec>(player(),0x30);static uint64_t lastActions{},lastMap{},lastCover{};auto now=GetTickCount64();
    if(now-lastActions>800){lastActions=now;nearby.clear();auto table=global(0x11C54C0);auto buckets=table?at<void**>(table,8):nullptr;auto count=table?at<uint32_t>(table,4):0;
        if(buckets&&count<1000000)for(uint32_t i=0;i<count;i++)for(auto e=buckets[i];e;e=at<void*>(e,0)){
            auto ref=at<void*>(e,8);if(!interactable(ref)||(at<uint32_t>(ref,8)&0x820))continue;
            auto render=at<void*>(ref,0x64);if(!render||!at<void*>(render,0x14))continue;
            auto pos=at<Vec>(ref,0x30);float d=length(pos-p);if(ref!=player()&&at<void*>(ref,0x40)==at<void*>(player(),0x40)&&std::abs(pos.z-p.z)<150&&d<350)nearby.push_back({at<uint32_t>(ref,0xC),pos,actionName(ref),d});
        }
        std::sort(nearby.begin(),nearby.end(),[](auto& a,auto& b){return a.range<b.range;});if(nearby.size()>8)nearby.resize(8);
    }
    if(now-lastCover>120){lastCover=now;restoreCover();Vec start=p+Vec{0,0,100},dir=normalized(cameraPos-start),hit{};void* object{};
        if(raycast(start,dir,hit,object)&&length(hit-start)<length(cameraPos-start)-40){auto ref=parentReference(object);if(ref&&ref!=player()){
            auto base=at<void*>(ref,0x20);auto type=base?at<uint8_t>(base,4):0;auto render=at<void*>(ref,0x64);auto node=render?at<void*>(render,0x14):nullptr;
            if(node&&(type==0x20||type==0x21||type==0x22)&&!(at<uint32_t>(node,0x30)&1)){occludingCover.push_back({at<uint32_t>(ref,0xC),node});}
        }}
    }

}
void nativeInteraction(){
    static void* owner{};static void* label{};static bool attempted{};
    auto hud=global(0x11D96C0);auto rootTile=hud?at<void*>(hud,4):nullptr;
    if(rootTile!=owner){owner=rootTile;label=nullptr;attempted=false;}
    actionBoxes.clear();if(!rootTile)return;
    if(!label&&!attempted&&active()){
        attempted=true;label=reinterpret_cast<void*(__thiscall*)(void*,const char*)>(0xA01B00)(rootTile,"menus\\MojaveIso\\interaction.xml");
        log(label?"Native interaction tile created":"Native interaction tile unavailable");
    }
    if(!label)return;
    auto set=[&](uint32_t id,float value){reinterpret_cast<void(__thiscall*)(void*,uint32_t,float,bool)>(0xA012D0)(label,id,value,true);};
    set(0xFA3,0);
    if(!active()||altAiming||attack.ordered||!hoverRef||!displayCamera.valid)return;
    auto ref=reference(hoverRef);if(!interactable(ref)||ref==player()||at<void*>(ref,0x40)!=at<void*>(player(),0x40))return;
    auto p=at<Vec>(ref,0x30),delta=p-at<Vec>(player(),0x30);
    if(length(delta)>350||std::abs(delta.z)>150)return;
    float x{},y{};if(!displayCamera.project(p+Vec{0,0,65},x,y))return;
    auto get=[&](void* tile,uint32_t id,float fallback){auto value=tileValue(tile,id);return value?at<float>(value,8):fallback;};
    float uiWidth=get(rootTile,0xFB1,1280),uiHeight=get(rootTile,0xFB0,720);
    if(uiWidth<=0||uiHeight<=0)return;
    auto nativeText=at<void*>(hud,0xA8);
    for(uint32_t id:{0xFB9u,0xFB2u,0xFB3u,0xFB4u})set(id,get(nativeText,id,id==0xFB9?3.f:255.f));
    auto text=actionName(ref);auto stringValue=tileValue(label,0xFC4);
    if(stringValue)reinterpret_cast<void(__thiscall*)(void*,const char*,bool)>(0xA0A300)(stringValue,text.c_str(),true);
    // Place one native-font prompt beside the hovered object, above HUD meters.
    x=std::clamp(x+12,8.f,std::max(8.f,width-230));y=std::clamp(y-24,8.f,std::max(8.f,height-110));
    set(0xFA1,x*uiWidth/width);set(0xFA2,y*uiHeight/height);set(0xFA3,1);
    actionBoxes.push_back({hoverRef,x,y,220,30});
}
void drawWorldUI(IDirect3DDevice9* device){
    if(fadeAlpha>.001f&&fadePainter.begin(device)){fadePainter.rect(0,0,width,height,DWORD(std::clamp(fadeAlpha,0.f,1.f)*255)<<24);fadePainter.finish();}
}
void present(){
    auto now=GetTickCount64();frameDt=lastFrameTick?std::clamp(float(now-lastFrameTick)/1000.f,0.f,.05f):.016f;lastFrameTick=now;
    bool gameplay=active();ownControls(gameplay);
    static bool previouslyActive{};
    if(gameplay!=previouslyActive&&!pendingActivation&&!pendingDisable){fadeAlpha=1;fadeHoldUntil=now+120;}previouslyActive=gameplay;
    float fadeGoal=(pendingActivation||pendingDisable||now<fadeHoldUntil)?1.f:0.f;
    float fadeStep=frameDt/.22f;fadeAlpha=fadeGoal>fadeAlpha?std::min(fadeGoal,fadeAlpha+fadeStep):std::max(fadeGoal,fadeAlpha-fadeStep);
    if(!gameplay){mouseDeltaX=0;mouseDeltaY=0;displayCamera.valid=false;wheelInput.reset();if(!pendingActivation)stop();}
    float goal=gameplay?1.f:0.f,step=frameDt/0.45f;
    cameraBlend=goal>cameraBlend?std::min(goal,cameraBlend+step):std::max(goal,cameraBlend-step);
    pitch+=(desiredPitch-pitch)*(1-std::exp(-12.f*frameDt));
    yaw+=std::remainder(desiredYaw-yaw,360.f)*(1-std::exp(-12.f*frameDt));
    if(!gameplay){lastL=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;lastR=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;}
    ++frames;void* renderer=global(0x11F4748);if(!renderer)return;auto device=at<IDirect3DDevice9*>(renderer,0x288);if(!device)return;
    // Cutaway remains disabled pending visibility regression diagnosis.
    D3DVIEWPORT9 vp{};if(SUCCEEDED(device->GetViewport(&vp))){width=float(vp.Width);height=float(vp.Height);}
    if(active()){
        destinationMarker(device);
        if(attack.ordered&&attack.id){float sx{},sy{};if(renderCamera.project(attack.point,sx,sy)){
            std::array<D3DRECT,24> marks{};DWORD count=0;
            for(int i=0;i<24;i++){float a=i*6.2831853f/24;LONG x=LONG(sx+std::cos(a)*16),y=LONG(sy+std::sin(a)*16);
                if(x>=1&&y>=1&&x<LONG(width)-2&&y<LONG(height)-2)marks[count++]={x-1,y-1,x+2,y+2};}
            if(count)device->Clear(count,marks.data(),D3DCLEAR_TARGET,hudColour(),1,0);
        }}
        // D3D9 Clear rects work outside BeginScene and do not alter shader state.
        LONG x=LONG(cursorX),y=LONG(cursorY);std::array<D3DRECT,13> arrow{};DWORD count=0;
        for(LONG i=0;i<12;i++){LONG rightEdge=std::min(LONG(width),x+1+i/2),bottom=std::min(LONG(height),y+i+1);if(y+i>=LONG(height))break;arrow[count++]={x,y+i,rightEdge,bottom};}
        if(altAiming){
            std::array<D3DRECT,4> reticle{{{x-10,y-1,x-3,y+2},{x+4,y-1,x+11,y+2},{x-1,y-10,x+2,y-3},{x-1,y+4,x+2,y+11}}};
            for(auto rect:reticle){rect.x1=std::max(0L,rect.x1);rect.y1=std::max(0L,rect.y1);rect.x2=std::min(LONG(width),rect.x2);rect.y2=std::min(LONG(height),rect.y2);if(rect.x2>rect.x1&&rect.y2>rect.y1)device->Clear(1,&rect,D3DCLEAR_TARGET,hudColour(),1,0);}
        }else if(count)device->Clear(count,arrow.data(),D3DCLEAR_TARGET,hudColour(),1,0);
        displayCamera=renderCamera;
    }
    drawWorldUI(device);
    if(captureRequested){captureRequested=false;capture(device);}
}
void saveSettings(){
    static std::string previous;static uint64_t lastSave{};
    auto now=GetTickCount64();if(now-lastSave<1000)return;lastSave=now;
    std::ostringstream text;text<<"[startup]\nauto_enable="<<(autoEnable?1:0)<<"\n[camera]\nyaw="<<std::remainder(desiredYaw,360.f)<<"\npitch="<<desiredPitch<<"\ndistance="<<distance<<"\nspan="<<span<<"\northographic="<<(orthographic?1:0)<<"\n";
    if(text.str()==previous)return;
    {std::ofstream file(bridge+"/settings.tmp");file<<text.str();if(!file)return;}
    if(MoveFileExA((bridge+"/settings.tmp").c_str(),(bridge+"/settings.ini").c_str(),MOVEFILE_REPLACE_EXISTING))previous=text.str();
}
void loadSettings(){
    const auto file=bridge+"/settings.ini";
    auto read=[&](const char* key,float fallback,float low,float high){
        char value[64];GetPrivateProfileStringA("camera",key,"",value,64,file.c_str());char* end{};float n=strtof(value,&end);
        return end!=value&&std::isfinite(n)?std::clamp(n,low,high):fallback;
    };
    desiredYaw=yaw=read("yaw",45,-360,360);desiredPitch=pitch=read("pitch",50,20,80);
    distance=read("distance",1100,200,4000);span=read("span",1500,300,5000);
    orthographic=GetPrivateProfileIntA("camera","orthographic",1,file.c_str())!=0;
    autoEnable=GetPrivateProfileIntA("startup","auto_enable",1,file.c_str())!=0;requested=autoEnable;
}
void onMessage(Message* m){
    // These ordinals are the public xNVSE 6.4.8 messaging ABI.
    switch(m->type){
    case 9:installHooks();break; // PostPostLoad
    case 2:case 6:restoreCover();mapReady=false;nearby.clear();pendingDisable=false;wheelInput.reset();saveSettings();setEnabled(false);requested=autoEnable;autoReadySince=0;camera=nullptr;savedCamera=nullptr;haveRenderedFrustum=false;break;
    case 14:requested=autoEnable;autoReadySince=0;break; // NewGame
    case 19:expressions.clear();break; // ClearScriptDataCache
    case 1:case 7:saveSettings();break; // Exit
    case 20:{static uint64_t previousTick{};auto now=GetTickCount64();if(previousTick&&now-previousTick>500)stop();previousTick=now;
        if(pendingActivation&&fadeAlpha>=.99f&&now-activationStarted>=250){
            auto ref=reference(interactionId);pendingActivation=false;interactionId=0;fadeHoldUntil=now+250;
            if(gameMode()&&interactable(ref)&&at<void*>(ref,0x40)==at<void*>(player(),0x40)&&length(target-at<Vec>(player(),0x30))<200){
                auto delta=target-at<Vec>(player(),0x30);at<float>(player(),0x2C)=atan2f(delta.x,delta.y);
                log("Activating reference "+std::to_string(at<uint32_t>(ref,0xC)));note=activate(ref)?"Interaction submitted to game":"Game declined interaction";
            }
        }
        if(pendingDisable&&fadeAlpha>=.99f){pendingDisable=false;setEnabled(false);fadeHoldUntil=now+150;}
        installMouseHook();
        if(requested&&!enabled&&hooksReady&&reference(0x14)==player()&&gameMode()&&!dialogue()&&player()&&at<void*>(player(),0x40)&&at<void*>(player(),0x64)){
            if(!autoReadySince)autoReadySince=now;
            if(now-autoReadySince>=750){setEnabled(true);autoReadySince=0;}
        }else autoReadySince=0;
        updateReticle(active());poll();input();sampleWorld();nativeInteraction();saveSettings();break;} // MainGameLoop
    case 24:present();break; // OnFramePresent
    }
}
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* api,PluginInfo* info){
    info->infoVersion=1;info->name="MojaveIsoNative";info->version=6;
    return !api->isEditor&&!api->isNogore&&api->runtimeVersion==0x040020D0;
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface* api){
    root=std::string(api->GetRuntimeDirectory())+"IsometricModdingTool";bridge=root+"/runtime";
    CreateDirectoryA(root.c_str(),nullptr);CreateDirectoryA(bridge.c_str(),nullptr);loadSettings();
    lastSequence=GetPrivateProfileIntA("command","sequence",0,(bridge+"/command.ini").c_str());
    console=static_cast<Console*>(api->QueryInterface(1));scripts=static_cast<Scripts*>(api->QueryInterface(6));
    auto messages=static_cast<Messaging*>(api->QueryInterface(2));
    if(!console||!scripts||!messages)return false;log("MojaveIsoNative 0.6 starting, native ABI 1.4.0.525");
    return messages->RegisterListener(api->GetPluginHandle(),"NVSE",onMessage);
}
