// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#include "../src/display_settings.hpp"
#include <fstream>
#include <iostream>
void check(bool ok,const char* what){if(!ok){std::cerr<<what<<"\n";exit(1);}}
int main(){namespace fs=std::filesystem;auto r=fs::temp_directory_path()/(L"MojaveDisplayTest-"+std::to_wstring(GetCurrentProcessId()));fs::create_directories(r/L"runtime");auto prefs=r/L"FalloutPrefs.ini",pending=r/L"runtime/display-pending.ini";
 {std::ofstream f(prefs);f<<"[Display]\niSize W=1024\niSize H=576\niMultiSample=0\nCustomSetting=42\n[Unrelated]\nKeepMe=yes\n";}
 check(display_settings::applyPending(r,prefs),"No-op should succeed");
 {std::ofstream f(pending);f<<"[Display]\niSize W=1280\niSize H=720\niMultiSample=4\nCustomSetting=999\n[Unrelated]\nKeepMe=no\n";}
 check(!display_settings::applyPending(r,r/L"missing.ini"),"Missing preferences must fail");check(fs::exists(pending),"Failure must retain pending changes");
 check(display_settings::applyPending(r,prefs),"Apply should succeed");check(!fs::exists(pending),"Pending consumed only on success");
 check(display_settings::read(prefs,L"Display",L"iSize W")==L"1280","Resolution width");check(display_settings::read(prefs,L"Display",L"iSize H")==L"720","Resolution height");check(display_settings::read(prefs,L"Display",L"iMultiSample")==L"4","AA");
 check(display_settings::read(prefs,L"Display",L"CustomSetting")==L"42","Only approved fields changed");check(display_settings::read(prefs,L"Unrelated",L"KeepMe")==L"yes","Unrelated sections preserved");
 auto backup=fs::directory_iterator(r/L"backups")->path();check(display_settings::read(backup,L"Display",L"iSize W")==L"1024","Original backed up");
 std::cout<<"Display persistence tests passed; isolated files: "<<r<<"\n";
}
