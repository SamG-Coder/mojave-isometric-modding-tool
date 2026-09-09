// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
namespace dlss_appearance {
inline constexpr auto section=L"RenoDX.DLSS5";
struct Option {const char* label;const wchar_t* key;const wchar_t* fallback;int low,high,step;std::vector<std::string> labels;};
// Values are hundredths, except enumerations with explicit labels.
inline const std::vector<Option>& options(){
 static const std::vector<Option> list{
  {"DLSS style *",L"NRStyle",L"0",0,2,1,{"Default","Natural","Cinematic"}},
  {"DLSS neural preset *",L"NRPreset",L"0",0,3,1,{"Default","Preset 1","Preset 2","Preset 3"}},
  {"DLSS overall intensity *",L"NRIntensity",L"1",0,200,10,{}},
  {"DLSS local tone *",L"NRLocalTone",L"1",0,200,10,{}},
  {"DLSS structure intensity *",L"NRLocalStructure",L"1",0,200,10,{}},
  {"DLSS skin structure *",L"NRSkinStructure",L"-1",-100,100,10,{}},
  {"DLSS character mask *",L"NRAutoMask",L"1",0,1,1,{"Off","On"}},
  {"DLSS UI correction *",L"NRUICorrection",L"1",0,1,1,{"Off","On"}},
  {"DLSS colour strength *",L"NRColorStrength",L"1",0,100,10,{}},
  {"DLSS paper-white scale *",L"NRPaperWhiteScale",L"1",0,1000,25,{}},
  {"DLSS HDR transfer strength *",L"NRTransferStrength",L"1",0,100,10,{}}
 };return list;
}
inline std::wstring read(const std::filesystem::path& file,const wchar_t* key,const wchar_t* fallback=L""){
 wchar_t value[128]{};GetPrivateProfileStringW(section,key,fallback,value,128,file.c_str());return value;
}
inline std::filesystem::path preferences(const std::filesystem::path& directory){return directory/L"appearance.ini";}
inline std::filesystem::path hostIni(const std::filesystem::path& directory){return directory/L"candidate/host64/ReShade.ini";}
inline std::wstring selected(const std::filesystem::path& directory,const Option& option){
 auto value=read(preferences(directory),option.key);return value.empty()?read(hostIni(directory),option.key,option.fallback):value;
}
// Apply only explicit user overrides before starting the helper. Writing its INI
// while ReShade is running would race with ReShade saving its own settings.
inline bool applyBeforeLaunch(const std::filesystem::path& directory){
 for(const auto& option:options()){
  auto value=read(preferences(directory),option.key);if(value.empty())continue;
  if(!WritePrivateProfileStringW(section,option.key,value.c_str(),hostIni(directory).c_str()))return false;
 }
 return true;
}
}
