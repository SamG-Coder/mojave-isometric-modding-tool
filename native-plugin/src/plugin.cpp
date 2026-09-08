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
using namespace engine;
namespace {
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
void stop(){interactionId=0;pendingActivation=false;if(moving)movement(0);if(heldForward>=0){run("ReleaseKey "+std::to_string(heldForward));heldForward=-1;}moving=false;}
void restoreProjection(){if(savedCamera){at<Frustum>(savedCamera,0xDC)=savedFrustum;savedCamera=nullptr;}}
void ownControls(bool acquire){
    if(acquire==controlsAcquired)return;
    for(int i=0;i<3;i++){
        if(acquire){ownedControls[i]=number("IsControlDisabled "+std::to_string(controls[i]))==0;if(ownedControls[i])run("DisableControl "+std::to_string(controls[i]));}
        else if(ownedControls[i]){run("EnableControl "+std::to_string(controls[i]));ownedControls[i]=false;}
    }
    controlsAcquired=acquire;
}
void updateReticle(bool hide);
void setEnabled(bool value){
    if(value==enabled)return;
    wheelInput.reset();mouseDeltaX=0;mouseDeltaY=0;
    if(value&&(!hooksReady||!player()||!gameMode())){note="Load a game before enabling the camera";return;}
    if(!value){updateReticle(false);displayCamera.valid=renderCamera.valid=false;stop();restoreProjection();enabled=false;ownControls(false);cameraBlend=0;if(priorThird==false&&player())reinterpret_cast<bool(__thiscall*)(void*,bool)>(0x950110)(player(),true);note="Normal controls restored";return;}
    priorThird=at<uint8_t>(player(),0x64C)!=0;if(!priorThird)reinterpret_cast<bool(__thiscall*)(void*,bool)>(0x950110)(player(),false);
    ownControls(true);
    enabled=true;cursorX=width/2;cursorY=height/2;note="Isometric prototype enabled";
}
bool active(){return enabled&&gameMode()&&!dialogue()&&player()&&at<uint8_t>(player(),0x64C)&&!pendingActivation;}
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
using SetupCamera=void(__thiscall*)(void*,Vec*,Vec*,Vec*,Vec*,Frustum*,float*);
SetupCamera originalSetupCamera{};
void __fastcall setupCameraHook(void* renderer,void*,Vec* pos,Vec* dir,Vec* camUp,Vec* camRight,Frustum* frustum,float* viewport){
    // Scope to the actual world camera; menu, shadow and reflection passes keep their own projection.
    if(enabled&&cameraBlend>0&&camera&&pos&&frustum&&length(*pos-cameraPos)<1.f){
        Frustum custom=*frustum;
        if(orthographic&&cameraBlend>0.999f){custom.ortho=true;custom.left=-span/2;custom.right=span/2;custom.top=span*height/width/2;custom.bottom=-custom.top;custom.nearPlane=5;}
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
    cameraPos=at<Vec>(player(),0x30)+Vec{0,0,65}-forward*distance;
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
        if(orthographic&&cameraBlend>0.999f){f.ortho=true;f.left=-span/2;f.right=span/2;f.top=span*height/width/2;f.bottom=-f.top;f.nearPlane=5;f.farPlane=std::max(savedFrustum.farPlane,24000.f);}
        else f=savedFrustum;
    }
}
uintptr_t callTarget(uintptr_t a){if(*reinterpret_cast<uint8_t*>(a)!=0xe8)return 0;return a+5+*reinterpret_cast<int32_t*>(a+1);}
bool patchCall(uintptr_t address,void* hookFunction){
    DWORD old{};if(!VirtualProtect(reinterpret_cast<void*>(address),5,PAGE_EXECUTE_READWRITE,&old))return false;
    *reinterpret_cast<int32_t*>(address+1)=int32_t(reinterpret_cast<uintptr_t>(hookFunction)-address-5);
    DWORD ignored;VirtualProtect(reinterpret_cast<void*>(address),5,old,&ignored);FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(address),5);return true;
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
    return raycast(origin,ray,hit,hitObject);
}
void destination(float x,float y){
    if(!active())return;
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
    stop();interactionId=pickedId;target=hit;markerUntil=GetTickCount64()+3000;moving=true;moveStarted=lastProgress=GetTickCount64();lastPos=pos;
    void* inputState=global(0x11F35CC);heldForward=inputState?at<uint8_t>(inputState,0x1B94):-1;
    if(heldForward>=0&&heldForward<255)run("HoldKey "+std::to_string(heldForward));else {heldForward=-1;stop();note="Forward movement is not bound to a keyboard key";return;}
    note=interactionId?"Walking to interact":"Walking to ground destination";
}
void walk(){
    if(!moving)return;if(!active()){stop();return;}
    Vec pos=at<Vec>(player(),0x30),delta=target-pos;
    if(interactionId&&(!interactable(reference(interactionId))||at<void*>(reference(interactionId),0x40)!=at<void*>(player(),0x40))){stop();note="Interaction target no longer available";return;}
    float reach=interactionId?110.f:24.f;
    if(std::sqrt(delta.x*delta.x+delta.y*delta.y)<reach&&std::abs(delta.z)<(interactionId?150.f:45.f)){
        uint32_t id=interactionId;stop();markerUntil=GetTickCount64()+900;
        if(id){interactionId=id;pendingActivation=true;activationStarted=GetTickCount64();ownControls(false);note="Approaching interaction camera";}
        else note="Destination reached";
        return;
    }
    auto now=GetTickCount64();if(length(pos-lastPos)>12){lastPos=pos;lastProgress=now;}
    if(now-lastProgress>1800||now-moveStarted>30000){stop();note="Movement stopped: blocked or timed out";return;}
    at<float>(player(),0x2c)=atan2f(delta.x,delta.y);
    movement(1|512);
}
void status(){
    std::ostringstream s;s<<"{\"pid\":"<<GetCurrentProcessId()<<",\"frames\":"<<frames<<",\"camera_updates\":"<<cameraUpdates<<",\"hooks_ready\":"<<(hooksReady?"true":"false")<<",\"enabled\":"<<(enabled?"true":"false")<<",\"game_mode\":"<<(gameMode()?"true":"false")<<",\"orthographic\":"<<(orthographic?"true":"false")<<",\"moving\":"<<(moving?"true":"false")<<",\"width\":"<<width<<",\"height\":"<<height<<",\"cursor\":["<<cursorX<<","<<cursorY<<"],\"target\":["<<target.x<<","<<target.y<<","<<target.z<<"]";
    s<<",\"camera_blend\":"<<cameraBlend<<",\"yaw\":"<<yaw<<",\"controls_owned\":"<<(controlsAcquired?"true":"false")<<",\"dialogue\":"<<(dialogue()?"true":"false")<<",\"interaction_id\":"<<interactionId<<",\"hover_ref\":"<<hoverRef<<",\"pitch\":"<<pitch<<",\"pick_error_pixels\":"<<lastPickError;
    if(player()){auto p=at<Vec>(player(),0x30);s<<",\"player\":["<<p.x<<","<<p.y<<","<<p.z<<"]";}
    s<<",\"renderer_hook_ready\":"<<(rendererHookReady?"true":"false")<<",\"projection_updates\":"<<projectionUpdates<<",\"rendered_orthographic\":"<<(haveRenderedFrustum&&renderedFrustum.ortho?"true":"false")<<",\"note\":\""<<note<<"\",\"error\":\""<<lastError<<"\"}";
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
        if(!strcmp(op,"enable")){requested=true;setEnabled(true);}else if(!strcmp(op,"disable")){requested=false;setEnabled(false);}
        else if(!strcmp(op,"capture"))captureRequested=true;
        else if(!strcmp(op,"stop"))stop();
        else if(!strcmp(op,"inspect")){void* object{};Vec point{};hoverRef=0;if(pick(setting("x",cursorX),setting("y",cursorY),point,object)){auto ref=parentReference(object);hoverRef=ref?at<uint32_t>(ref,0xC):0;}status();}
        else if(!strcmp(op,"move"))destination(setting("x",width/2),setting("y",height/2));
        else if(!strcmp(op,"console")){char line[512];GetPrivateProfileStringA("command","text","",line,512,file.c_str());if(*line){run(line);note="Console command submitted; inspect frame for result";}}
    }
    if(now-lastStatus>500){lastStatus=now;status();}
}
void input(){
    DWORD foreground{};GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    if(foreground!=GetCurrentProcessId()){mouseDeltaX=0;mouseDeltaY=0;stop();lastL=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;lastR=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;return;}
    bool f=(GetAsyncKeyState(VK_F8)&0x8000)!=0;if(f&&!lastF){requested=!enabled;setEnabled(requested);}lastF=f;
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
    if(l&&!lastL&&!lastMiddle)destination(cursorX,cursorY);if(r&&!lastR)stop();lastL=l;lastR=r;
    static uint64_t lastHover{};auto now=GetTickCount64();
    if(now-lastHover>100){lastHover=now;hoverRef=0;Vec hit{};void* object{};if(pick(cursorX,cursorY,hit,object)){auto ref=parentReference(object);if(interactable(ref))hoverRef=at<uint32_t>(ref,0xC);}}
    if(GetAsyncKeyState(VK_ESCAPE)&0x8000)stop();walk();
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
    if(!moving&&GetTickCount64()>markerUntil)return;
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
void present(){
    auto now=GetTickCount64();frameDt=lastFrameTick?std::clamp(float(now-lastFrameTick)/1000.f,0.f,.05f):.016f;lastFrameTick=now;
    bool gameplay=active();ownControls(gameplay);
    if(!gameplay){mouseDeltaX=0;mouseDeltaY=0;displayCamera.valid=false;wheelInput.reset();if(!pendingActivation)stop();}
    float goal=gameplay?1.f:0.f,step=frameDt/0.45f;
    cameraBlend=goal>cameraBlend?std::min(goal,cameraBlend+step):std::max(goal,cameraBlend-step);
    pitch+=(desiredPitch-pitch)*(1-std::exp(-12.f*frameDt));
    yaw+=std::remainder(desiredYaw-yaw,360.f)*(1-std::exp(-12.f*frameDt));
    if(!gameplay){lastL=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;lastR=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;}
    ++frames;void* renderer=global(0x11F4748);if(!renderer)return;auto device=at<IDirect3DDevice9*>(renderer,0x288);if(!device)return;
    D3DVIEWPORT9 vp{};if(SUCCEEDED(device->GetViewport(&vp))){width=float(vp.Width);height=float(vp.Height);}
    if(active()){
        destinationMarker(device);
        // D3D9 Clear rects work outside BeginScene and do not alter shader state.
        LONG x=LONG(cursorX),y=LONG(cursorY);std::array<D3DRECT,13> arrow{};DWORD count=0;
        for(LONG i=0;i<12;i++){LONG rightEdge=std::min(LONG(width),x+1+i/2),bottom=std::min(LONG(height),y+i+1);if(y+i>=LONG(height))break;arrow[count++]={x,y+i,rightEdge,bottom};}
        if(count)device->Clear(count,arrow.data(),D3DCLEAR_TARGET,hudColour(),1,0);
        displayCamera=renderCamera;
    }
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
    case 2:case 6:wheelInput.reset();saveSettings();setEnabled(false);requested=autoEnable;autoReadySince=0;camera=nullptr;savedCamera=nullptr;haveRenderedFrustum=false;break;
    case 14:requested=autoEnable;autoReadySince=0;break; // NewGame
    case 19:expressions.clear();break; // ClearScriptDataCache
    case 1:case 7:saveSettings();break; // Exit
    case 20:{static uint64_t previousTick{};auto now=GetTickCount64();if(previousTick&&now-previousTick>500)stop();previousTick=now;
        if(pendingActivation&&now-activationStarted>=500){
            auto ref=reference(interactionId);pendingActivation=false;interactionId=0;
            if(gameMode()&&interactable(ref)&&at<void*>(ref,0x40)==at<void*>(player(),0x40)&&length(target-at<Vec>(player(),0x30))<200){
                auto delta=target-at<Vec>(player(),0x30);at<float>(player(),0x2C)=atan2f(delta.x,delta.y);
                note=activate(ref)?"Interaction submitted to game":"Game declined interaction";
            }
        }
        installMouseHook();
        if(requested&&!enabled&&hooksReady&&gameMode()&&!dialogue()&&player()&&at<void*>(player(),0x40)&&at<void*>(player(),0x64)){
            if(!autoReadySince)autoReadySince=now;
            if(now-autoReadySince>=750){setEnabled(true);autoReadySince=0;}
        }else autoReadySince=0;
        updateReticle(active());poll();input();saveSettings();break;} // MainGameLoop
    case 24:present();break; // OnFramePresent
    }
}
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* api,PluginInfo* info){
    info->infoVersion=1;info->name="MojaveIsoNative";info->version=5;
    return !api->isEditor&&!api->isNogore&&api->runtimeVersion==0x040020D0;
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface* api){
    root=std::string(api->GetRuntimeDirectory())+"IsometricModdingTool";bridge=root+"/runtime";
    CreateDirectoryA(root.c_str(),nullptr);CreateDirectoryA(bridge.c_str(),nullptr);loadSettings();
    lastSequence=GetPrivateProfileIntA("command","sequence",0,(bridge+"/command.ini").c_str());
    console=static_cast<Console*>(api->QueryInterface(1));scripts=static_cast<Scripts*>(api->QueryInterface(6));
    auto messages=static_cast<Messaging*>(api->QueryInterface(2));
    if(!console||!scripts||!messages)return false;log("MojaveIsoNative 0.5 starting, native ABI 1.4.0.525");
    return messages->RegisterListener(api->GetPluginHandle(),"NVSE",onMessage);
}
