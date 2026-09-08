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
using namespace engine;
namespace {
std::string root, bridge; Console* console{}; Scripts* scripts{};
bool enabled{},requested{},hooksReady{},moving{},captureRequested{},lastL{},lastF{},lastR{};
bool priorThird{}; std::array<bool,3> ownedControls{};
int heldForward=-1;
constexpr int controls[]{4,6,13};
float yaw=45,pitch=50,distance=1100,span=1500,cursorX=640,cursorY=360;
bool orthographic=false; float width=1280,height=720;
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
void stop(){if(moving)movement(0);if(heldForward>=0){run("ReleaseKey "+std::to_string(heldForward));heldForward=-1;}moving=false;}
void restoreProjection(){if(savedCamera){at<Frustum>(savedCamera,0xDC)=savedFrustum;savedCamera=nullptr;}}
void setEnabled(bool value){
    if(value==enabled)return;
    if(value&&(!hooksReady||!player()||!gameMode())){note="Load a game before enabling the camera";return;}
    if(!value){stop();restoreProjection();enabled=false;for(int i=0;i<3;i++)if(ownedControls[i]){run("EnableControl "+std::to_string(controls[i]));ownedControls[i]=false;}if(priorThird==false&&player())reinterpret_cast<bool(__thiscall*)(void*,bool)>(0x950110)(player(),true);note="Normal controls restored";return;}
    priorThird=at<uint8_t>(player(),0x64C)!=0;if(!priorThird)reinterpret_cast<bool(__thiscall*)(void*,bool)>(0x950110)(player(),false);
    for(int i=0;i<3;i++){ownedControls[i]=number("IsControlDisabled "+std::to_string(controls[i]))==0;if(ownedControls[i])run("DisableControl "+std::to_string(controls[i]));}
    enabled=true;cursorX=width/2;cursorY=height/2;note="Isometric prototype enabled";
}
bool active(){return enabled&&gameMode()&&player()&&at<uint8_t>(player(),0x64C);}
using SetupCamera=void(__thiscall*)(void*,Vec*,Vec*,Vec*,Vec*,Frustum*,float*);
SetupCamera originalSetupCamera{};
void __fastcall setupCameraHook(void* renderer,void*,Vec* pos,Vec* dir,Vec* camUp,Vec* camRight,Frustum* frustum,float* viewport){
    // Scope to the actual world camera; menu, shadow and reflection passes keep their own projection.
    if(active()&&camera&&pos&&frustum&&length(*pos-cameraPos)<1.f){
        Frustum custom=*frustum;
        if(orthographic){custom.ortho=true;custom.left=-span/2;custom.right=span/2;custom.top=span*height/width/2;custom.bottom=-custom.top;custom.nearPlane=5;}
        renderedFrustum=custom;haveRenderedFrustum=true;++projectionUpdates;
        originalSetupCamera(renderer,pos,dir,camUp,camRight,&custom,viewport);
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
    if(active()){basis();originalPos(node,&cameraPos);++cameraUpdates;}else originalPos(node,pos);
}
void __fastcall rotationHook(void* node,void*,const Mat* rot){
    if(!active()){restoreProjection();originalRot(node,rot);return;}
    originalRot(node,&rotation);camera=cameraChild(node);
    if(camera){
        if(savedCamera!=camera){savedCamera=camera;savedFrustum=at<Frustum>(camera,0xDC);}
        auto& f=at<Frustum>(camera,0xDC);
        if(orthographic){f.ortho=true;f.left=-span/2;f.right=span/2;f.top=span*height/width/2;f.bottom=-f.top;f.nearPlane=5;f.farPlane=std::max(savedFrustum.farPlane,24000.f);}
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
void destination(float x,float y){
    if(!active()||!camera)return;
    float sx=2*x/width-1,sy=1-2*y/height;Vec origin=cameraPos,ray=forward;
    auto f=haveRenderedFrustum?renderedFrustum:at<Frustum>(camera,0xDC);
    if(f.ortho)origin=origin+right*(sx*span/2)+up*(sy*span*height/width/2);
    else ray=normalized(forward+right*(sx*f.right)+up*(sy*f.top));
    void* hitObject{};Vec hit{};
    if(!raycast(origin,ray,hit,hitObject)){note="No collision surface under cursor";stop();return;}
    Vec pos=at<Vec>(player(),0x30);if(length(hit-pos)>6000){note="Destination too far away";stop();return;}
    // Reject high roof/wall hits. Navigation is deliberately a straight-line prototype.
    if(hit.z-pos.z>100){note="Destination is above player: choose ground";stop();return;}
    stop();target=hit;moving=true;moveStarted=lastProgress=GetTickCount64();lastPos=pos;
    void* inputState=global(0x11F35CC);heldForward=inputState?at<uint8_t>(inputState,0x1B94):-1;
    if(heldForward>=0&&heldForward<255)run("HoldKey "+std::to_string(heldForward));else {heldForward=-1;stop();note="Forward movement is not bound to a keyboard key";return;}
    note="Walking to collision hit (obstacle routing not implemented)";
}
void walk(){
    if(!moving)return;if(!active()){stop();return;}
    Vec pos=at<Vec>(player(),0x30),delta=target-pos;
    if(std::sqrt(delta.x*delta.x+delta.y*delta.y)<24){stop();note="Destination reached";return;}
    auto now=GetTickCount64();if(length(pos-lastPos)>12){lastPos=pos;lastProgress=now;}
    if(now-lastProgress>1800||now-moveStarted>30000){stop();note="Movement stopped: blocked or timed out";return;}
    at<float>(player(),0x2c)=atan2f(delta.x,delta.y);
    movement(1|512);
}
void status(){
    std::ostringstream s;s<<"{\"pid\":"<<GetCurrentProcessId()<<",\"frames\":"<<frames<<",\"camera_updates\":"<<cameraUpdates<<",\"hooks_ready\":"<<(hooksReady?"true":"false")<<",\"enabled\":"<<(enabled?"true":"false")<<",\"game_mode\":"<<(gameMode()?"true":"false")<<",\"orthographic\":"<<(orthographic?"true":"false")<<",\"moving\":"<<(moving?"true":"false")<<",\"width\":"<<width<<",\"height\":"<<height<<",\"cursor\":["<<cursorX<<","<<cursorY<<"],\"target\":["<<target.x<<","<<target.y<<","<<target.z<<"]";
    if(player()){auto p=at<Vec>(player(),0x30);s<<",\"player\":["<<p.x<<","<<p.y<<","<<p.z<<"]";}
    s<<",\"renderer_hook_ready\":"<<(rendererHookReady?"true":"false")<<",\"projection_updates\":"<<projectionUpdates<<",\"rendered_orthographic\":"<<(haveRenderedFrustum&&renderedFrustum.ortho?"true":"false")<<",\"note\":\""<<note<<"\",\"error\":\""<<lastError<<"\"}";
    {std::ofstream f(bridge+"/status.tmp");f<<s.str();}MoveFileExA((bridge+"/status.tmp").c_str(),(bridge+"/status.json").c_str(),MOVEFILE_REPLACE_EXISTING);
}
float setting(const char* name,float fallback){char value[64];GetPrivateProfileStringA("camera",name,std::to_string(fallback).c_str(),value,64,(bridge+"/command.ini").c_str());char* end{};float n=strtof(value,&end);return end!=value&&std::isfinite(n)?n:fallback;}
void poll(){
    const auto now=GetTickCount64();if(now-lastPoll<100)return;lastPoll=now;
    const std::string file=bridge+"/command.ini";
    auto seq=GetPrivateProfileIntA("command","sequence",0,file.c_str());if(seq&&seq!=lastSequence){
        lastSequence=seq;yaw=setting("yaw",yaw);pitch=std::clamp(setting("pitch",pitch),20.f,80.f);distance=std::clamp(setting("distance",distance),200.f,4000.f);span=std::clamp(setting("span",span),300.f,5000.f);
        orthographic=GetPrivateProfileIntA("camera","orthographic",orthographic,file.c_str())!=0;
        char op[40];GetPrivateProfileStringA("command","operation","configure",op,40,file.c_str());
        if(!strcmp(op,"enable")){requested=true;setEnabled(true);}else if(!strcmp(op,"disable")){requested=false;setEnabled(false);}
        else if(!strcmp(op,"capture"))captureRequested=true;
        else if(!strcmp(op,"stop"))stop();
        else if(!strcmp(op,"move"))destination(setting("x",width/2),setting("y",height/2));
        else if(!strcmp(op,"console")){char line[512];GetPrivateProfileStringA("command","text","",line,512,file.c_str());if(*line){run(line);note="Console command submitted; inspect frame for result";}}
    }
    if(now-lastStatus>500){lastStatus=now;status();}
}
void input(){
    DWORD foreground{};GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
    if(foreground!=GetCurrentProcessId()){stop();return;}
    bool f=(GetAsyncKeyState(VK_F8)&0x8000)!=0;if(f&&!lastF){requested=!enabled;setEnabled(requested);}lastF=f;
    if(!active()){stop();return;}
    void* in=global(0x11F35CC);if(in){cursorX=std::clamp(cursorX+float(at<int>(in,0x1b24)),0.f,width-1);cursorY=std::clamp(cursorY+float(at<int>(in,0x1b28)),0.f,height-1);
        int wheel=at<int>(in,0x1b2c);if(wheel){distance=std::clamp(distance-wheel*.5f,200.f,4000.f);span=std::clamp(span-wheel*.5f,300.f,5000.f);}}
    bool l=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0,r=(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0;
    if(l&&!lastL)destination(cursorX,cursorY);if(r&&!lastR)stop();lastL=l;lastR=r;
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
void present(){
    ++frames;void* renderer=global(0x11F4748);if(!renderer)return;auto device=at<IDirect3DDevice9*>(renderer,0x288);if(!device)return;
    D3DVIEWPORT9 vp{};if(SUCCEEDED(device->GetViewport(&vp))){width=float(vp.Width);height=float(vp.Height);}
    if(active()){
        // D3D9 Clear rects work outside BeginScene and do not alter shader state.
        LONG x=LONG(cursorX),y=LONG(cursorY);D3DRECT rects[2]={{std::max(0L,x-7),std::max(0L,y-1),std::min(LONG(width),x+8),std::min(LONG(height),y+2)},{std::max(0L,x-1),std::max(0L,y-7),std::min(LONG(width),x+2),std::min(LONG(height),y+8)}};
        device->Clear(2,rects,D3DCLEAR_TARGET,0xffffce59,1,0);
    }
    if(captureRequested){captureRequested=false;capture(device);}
}
void onMessage(Message* m){
    // These ordinals are the public xNVSE 6.4.8 messaging ABI.
    switch(m->type){
    case 9:installHooks();break; // PostPostLoad
    case 2:case 6:requested=false;setEnabled(false);camera=nullptr;savedCamera=nullptr;break;
    case 20:{static uint64_t previousTick{};auto now=GetTickCount64();if(previousTick&&now-previousTick>500)stop();previousTick=now;poll();input();break;} // MainGameLoop
    case 24:present();break; // OnFramePresent
    }
}
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* api,PluginInfo* info){
    info->infoVersion=1;info->name="MojaveIsoNative";info->version=1;
    return !api->isEditor&&!api->isNogore&&api->runtimeVersion==0x040020D0;
}
extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface* api){
    root=std::string(api->GetRuntimeDirectory())+"IsometricModdingTool";bridge=root+"/runtime";
    lastSequence=GetPrivateProfileIntA("command","sequence",0,(bridge+"/command.ini").c_str());
    console=static_cast<Console*>(api->QueryInterface(1));scripts=static_cast<Scripts*>(api->QueryInterface(6));
    auto messages=static_cast<Messaging*>(api->QueryInterface(2));
    if(!console||!scripts||!messages)return false;log("MojaveIsoNative 0.1 starting, native ABI 1.4.0.525");
    return messages->RegisterListener(api->GetPluginHandle(),"NVSE",onMessage);
}
