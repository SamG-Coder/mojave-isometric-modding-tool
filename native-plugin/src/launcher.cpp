// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <string>
#include "display_settings.hpp"
#include "experimental_rendering.hpp"
namespace fs=std::filesystem;
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR arguments,int){
    wchar_t path[32768]{};if(!GetModuleFileNameW(nullptr,path,32768))return 1;
    const auto root=fs::path(path).parent_path(),game=root.parent_path();
    auto fail=[](const wchar_t* message){MessageBoxW(nullptr,message,L"New Vegas Isometric",MB_OK|MB_ICONERROR);return 1;};
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return fail(L"Could not check whether New Vegas is already running.");
    PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(Process32FirstW(snapshot,&entry))do{if(!_wcsicmp(entry.szExeFile,L"FalloutNV.exe")){CloseHandle(snapshot);return 0;}}while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);
    try{
        const auto pending=root/L"runtime/MojaveIsoNative.pending.dll",installed=game/L"Data/NVSE/Plugins/MojaveIsoNative.dll";
        if(fs::exists(pending)){
            fs::create_directories(installed.parent_path());fs::create_directories(root/L"backups");
            if(fs::exists(installed))fs::copy_file(installed,root/L"backups"/(L"MojaveIsoNative-"+std::to_wstring(GetTickCount64())+L".dll"));
            fs::copy_file(pending,installed,fs::copy_options::overwrite_existing);fs::remove(pending);
        }
        if(!fs::exists(installed))return fail(L"The isometric plugin is missing. Install or rebuild the mod first.");
        if(!display_settings::applyPending(root))return fail(L"Could not apply display settings. Check that FalloutPrefs.ini is writable; your pending changes were retained.");
        const auto dlssDir=root/L"runtime/dlss5",aaBackup=dlssDir/L"display-backup.ini";
        const auto dlssMode=experimental_rendering::enabled ? GetPrivateProfileIntW(L"bridge",L"mode",0,(dlssDir/L"bridge.ini").c_str()) : 0;
        const auto prefs=display_settings::prefsPath();
        if(dlssMode){
            fs::create_directories(dlssDir);
            if(!fs::exists(aaBackup)&&!WritePrivateProfileStringW(L"Display",L"iMultiSample",display_settings::read(prefs,L"Display",L"iMultiSample",L"0").c_str(),aaBackup.c_str()))return fail(L"Could not preserve anti-aliasing settings for DLSS.");
            if(!WritePrivateProfileStringW(L"Display",L"iMultiSample",L"0",prefs.c_str()))return fail(L"Could not disable MSAA for the DLSS depth buffer.");
        }else if(fs::exists(aaBackup)){
            if(display_settings::read(prefs,L"Display",L"iMultiSample",L"0")==L"0"&&!WritePrivateProfileStringW(L"Display",L"iMultiSample",display_settings::read(aaBackup,L"Display",L"iMultiSample",L"0").c_str(),prefs.c_str()))return fail(L"Could not restore anti-aliasing settings.");
            fs::remove(aaBackup);
        }
        if(arguments&&std::wstring(arguments)==L"--rtx-off")WritePrivateProfileStringW(L"experimental",L"rtx_remix",L"0",(root/L"runtime/settings.ini").c_str());
        unsigned mode=experimental_rendering::enabled ? GetPrivateProfileIntW(L"experimental",L"rtx_remix",0,(root/L"runtime/settings.ini").c_str()) : 0;
        if(mode>2)mode=0;
        const wchar_t* names[]{L"Off",L"On",L"Setup"};
        if(mode||fs::exists(root/L"runtime/remix/active")){
            auto script=root/L"desktop/remix-runtime.ps1";
            if(!fs::exists(script))return fail(L"RTX setup script is missing. Restore desktop/remix-runtime.ps1.");
            wchar_t windows[32768]{};GetWindowsDirectoryW(windows,32768);
            auto powershell=fs::path(windows)/L"System32/WindowsPowerShell/v1.0/powershell.exe";
            std::wstring setup=L"\""+powershell.wstring()+L"\" -NoProfile -ExecutionPolicy Bypass -File \""+script.wstring()+L"\" -Mode "+names[mode];
            STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
            if(!CreateProcessW(powershell.c_str(),setup.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.c_str(),&si,&pi))return fail(L"Could not start RTX setup.");
            WaitForSingleObject(pi.hProcess,INFINITE);DWORD result{};GetExitCodeProcess(pi.hProcess,&result);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
            if(result)return fail(L"RTX setup stopped. See IsometricModdingTool/runtime/remix/error.txt. Existing conflicting renderers were preserved.");
        }
        SetEnvironmentVariableW(L"MOJAVE_RTX_MODE",std::to_wstring(mode).c_str());
        if(mode)SetEnvironmentVariableW(L"DXVK_RTX_CONFIG_FILE",(root/L"runtime/remix/profile/rtx.conf").c_str());
        else SetEnvironmentVariableW(L"DXVK_RTX_CONFIG_FILE",nullptr);
        const auto loader=game/L"nvse_loader.exe";
        if(!fs::exists(loader))return fail(L"xNVSE is missing from the New Vegas folder.");
        std::wstring command=L"\""+loader.wstring()+L"\"";
        STARTUPINFOW start{};start.cb=sizeof(start);PROCESS_INFORMATION process{};
        if(!CreateProcessW(loader.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,game.c_str(),&start,&process))return fail(L"Could not start xNVSE.");
        CloseHandle(process.hThread);CloseHandle(process.hProcess);return 0;
    }catch(const fs::filesystem_error&){return fail(L"Could not install the pending plugin update. Close New Vegas and try again.");}
}
