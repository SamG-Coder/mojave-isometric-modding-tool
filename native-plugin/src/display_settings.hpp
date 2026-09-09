// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#pragma once
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <string>
#include <vector>
namespace display_settings {
struct Option {const wchar_t* section;const wchar_t* key;const char* label;std::vector<std::wstring> values;std::vector<std::string> labels;};
inline std::vector<Option> options(){return {
 {L"Display",L"iMultiSample","Anti-aliasing",{L"0",L"2",L"4",L"8"},{"Off","2x","4x","8x"}},
 {L"Display",L"iMaxAnisotropy","Anisotropic filtering",{L"0",L"2",L"4",L"8",L"16"},{"Off","2x","4x","8x","16x"}},
 {L"Display",L"bFull Screen","Window mode",{L"0",L"1"},{"Windowed","Fullscreen"}},
 {L"Display",L"iPresentInterval","VSync",{L"0",L"1"},{"Off","On"}},
 {L"Display",L"iTexMipMapSkip","Texture quality",{L"2",L"1",L"0"},{"Low","Medium","High"}},
 {L"Display",L"bTransparencyMultisampling","Transparency AA",{L"0",L"1"},{"Off","On"}},
 {L"Display",L"bDoActorShadows","Actor shadows",{L"0",L"1"},{"Off","On"}},
 {L"Display",L"bDoStaticAndArchShadows","Object shadows",{L"0",L"1"},{"Off","On"}},
 {L"Display",L"iShadowMapResolution","Shadow resolution",{L"256",L"512",L"1024",L"2048"},{"256","512","1024","2048"}},
 {L"Display",L"iShadowFilter","Shadow filtering",{L"0",L"1",L"2"},{"Low","Medium","High"}},
 {L"BlurShaderHDR",L"bDoHighDynamicRange","HDR",{L"0",L"1"},{"Off","On"}},
 {L"Imagespace",L"bDoDepthOfField","Depth of field",{L"0",L"1"},{"Off","On"}},
 {L"Water",L"bUseWaterReflections","Water reflections",{L"0",L"1"},{"Off","On"}},
 {L"Water",L"bUseWaterRefractions","Water refractions",{L"0",L"1"},{"Off","On"}},
 {L"Water",L"bUseWaterDepth","Water depth",{L"0",L"1"},{"Off","On"}},
 {L"Water",L"bUseWaterHiRes","High resolution water",{L"0",L"1"},{"Off","On"}},
 {L"Water",L"bUseWaterDisplacements","Water displacement",{L"0",L"1"},{"Off","On"}},
 {L"Water",L"bUseWaterReflectionBlur","Water reflection blur",{L"0",L"1"},{"Off","On"}},
 {L"LOD",L"fLODFadeOutMultObjects","Object distance",{L"3",L"5",L"10",L"15"},{"Low","Medium","High","Ultra"}},
 {L"LOD",L"fLODFadeOutMultItems","Item distance",{L"2",L"5",L"10",L"15"},{"Low","Medium","High","Ultra"}},
 {L"LOD",L"fLODFadeOutMultActors","Actor distance",{L"3",L"5",L"10",L"15"},{"Low","Medium","High","Ultra"}},
 {L"Grass",L"fGrassStartFadeDistance","Grass distance",{L"1000",L"3000",L"5000",L"7000"},{"Low","Medium","High","Ultra"}},
 {L"TerrainManager",L"fTreeLoadDistance","Tree distance",{L"10000",L"25000",L"40000"},{"Low","Medium","High"}},
 {L"Display",L"fShadowLODStartFade","Shadow distance",{L"500",L"1000",L"2000",L"4000"},{"Low","Medium","High","Ultra"}},
 {L"Display",L"fLightLODStartFade","Light distance",{L"1000",L"2000",L"3500",L"5000"},{"Low","Medium","High","Ultra"}},
 {L"Display",L"fSpecularLODStartFade","Specular distance",{L"500",L"1000",L"2000",L"4000"},{"Low","Medium","High","Ultra"}}
 };}
inline std::filesystem::path prefsPath(){PWSTR docs{};if(FAILED(SHGetKnownFolderPath(FOLDERID_Documents,0,nullptr,&docs)))return {};std::filesystem::path p=docs;CoTaskMemFree(docs);return p/L"My Games/FalloutNV/FalloutPrefs.ini";}
inline std::wstring read(const std::filesystem::path& path,const wchar_t* section,const wchar_t* key,const wchar_t* fallback=L""){wchar_t value[1024]{};GetPrivateProfileStringW(section,key,fallback,value,1024,path.c_str());return value;}
inline bool applyPending(const std::filesystem::path& root,const std::filesystem::path& preferences={}){
 auto pending=root/L"runtime/display-pending.ini";if(!std::filesystem::exists(pending))return true;
 auto prefs=preferences.empty()?prefsPath():preferences;if(prefs.empty()||!std::filesystem::exists(prefs))return false;
 auto temp=prefs;temp+=L".mojave-tmp";
 std::filesystem::create_directories(root/L"backups");
 std::filesystem::copy_file(prefs,root/L"backups"/(L"Display-"+std::to_wstring(GetTickCount64())+L".ini"));
 std::filesystem::copy_file(prefs,temp,std::filesystem::copy_options::overwrite_existing);
 auto fields=options();for(auto key:{L"iAdapter",L"sD3DDevice",L"iSize W",L"iSize H"})fields.push_back({L"Display",key,"",{}, {}});
 for(auto& field:fields){auto value=read(pending,field.section,field.key);if(!value.empty()&&!WritePrivateProfileStringW(field.section,field.key,value.c_str(),temp.c_str()))return false;}
 WritePrivateProfileStringW(nullptr,nullptr,nullptr,temp.c_str());
 if(!MoveFileExW(temp.c_str(),prefs.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
 std::filesystem::remove(pending);return true;
}
}
