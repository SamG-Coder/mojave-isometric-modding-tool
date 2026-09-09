// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#pragma once
#include "experimental_rendering.hpp"
#include <d3d9on12.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
namespace renderer_bridge {
using Microsoft::WRL::ComPtr;
inline std::filesystem::path directory;
inline int mode{};
inline int debugView{}; // 0 processed image, 1 motion, 2 rejection mask
inline void record(const std::string& message){std::ofstream(directory/"bridge.log",std::ios::app)<<GetTickCount64()<<" "<<message<<"\n";}
inline IDirect3D9* WINAPI create(UINT sdk){
 auto dll=LoadLibraryExW(L"d3d9.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
 auto fn=dll?reinterpret_cast<PFN_Direct3DCreate9On12>(GetProcAddress(dll,"Direct3DCreate9On12")):nullptr;
 D3D9ON12_ARGS args{};args.Enable9On12=TRUE;
 auto api=fn?fn(sdk,&args,1):nullptr;
 record(api?"Windows D3D9On12 created":"D3D9On12 unavailable; using native D3D9");
 if(!api&&dll){auto fallback=reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(dll,"Direct3DCreate9"));if(fallback)api=fallback(sdk);}
 return api;
}
inline FARPROC WINAPI resolve(HMODULE module,LPCSTR name){
 if(reinterpret_cast<ULONG_PTR>(name)>65535&&module==GetModuleHandleW(L"d3d9.dll")&&!strcmp(name,"Direct3DCreate9"))return reinterpret_cast<FARPROC>(&create);
 return GetProcAddress(module,name);
}
inline void install(const std::filesystem::path& root){
 if(!experimental_rendering::enabled){mode=0;debugView=0;return;}
 directory=root/L"runtime/dlss5";std::filesystem::create_directories(directory);
 mode=GetPrivateProfileIntW(L"bridge",L"mode",0,(directory/L"bridge.ini").c_str());
 debugView=std::clamp(int(GetPrivateProfileIntW(L"bridge",L"debug_view",0,(directory/L"bridge.ini").c_str())),0,2);
 if(mode<1||mode>2){mode=0;return;}
 if(GetPrivateProfileIntW(L"experimental",L"rtx_remix",0,(root/L"runtime/settings.ini").c_str())||std::filesystem::exists(root.parent_path()/L"d3d9.dll")){
  record("Another renderer is enabled; independent bridge not installed");mode=0;return;
 }
 // Only redirect the game's named import, never another mod's device or system DLL.
 auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
 auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+dos->e_lfanew);
 auto rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
 if(!rva){record("No import table; bridge unavailable");return;}
 for(auto d=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+rva);d->Name;++d){
  if(!d->OriginalFirstThunk)continue;
  auto names=reinterpret_cast<IMAGE_THUNK_DATA*>(base+d->OriginalFirstThunk),slots=reinterpret_cast<IMAGE_THUNK_DATA*>(base+d->FirstThunk);
  for(;names->u1.AddressOfData;++names,++slots){
   if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))continue;
   auto name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
   auto symbol=reinterpret_cast<char*>(name->Name);
   bool direct=!_stricmp(reinterpret_cast<char*>(base+d->Name),"d3d9.dll")&&!strcmp(symbol,"Direct3DCreate9");
   bool dynamic=!_stricmp(reinterpret_cast<char*>(base+d->Name),"kernel32.dll")&&!strcmp(symbol,"GetProcAddress");
   if(!direct&&!dynamic)continue;
   DWORD old{};if(VirtualProtect(&slots->u1.Function,sizeof(void*),PAGE_READWRITE,&old)){
    slots->u1.Function=direct?reinterpret_cast<ULONG_PTR>(&create):reinterpret_cast<ULONG_PTR>(&resolve);DWORD ignored{};VirtualProtect(&slots->u1.Function,sizeof(void*),old,&ignored);record(direct?"Game Direct3DCreate9 import redirected":"Game dynamic Direct3DCreate9 lookup redirected");return;
   }
  }
 }
 record("Direct3DCreate9 import not found; bridge unavailable");
}
inline void probe(IDirect3DDevice9* device){
 static bool done{};if(!mode||done)return;done=true;
 ComPtr<IDirect3DDevice9On12> interop;auto hr=device->QueryInterface(IID_PPV_ARGS(&interop));
 record("Game D3D9On12 interface HRESULT="+std::to_string(static_cast<unsigned>(hr)));
}
}
