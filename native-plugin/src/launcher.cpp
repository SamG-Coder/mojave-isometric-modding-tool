// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <string>
namespace fs=std::filesystem;
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
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
        const auto loader=game/L"nvse_loader.exe";
        if(!fs::exists(loader))return fail(L"xNVSE is missing from the New Vegas folder.");
        std::wstring command=L"\""+loader.wstring()+L"\"";
        STARTUPINFOW start{};start.cb=sizeof(start);PROCESS_INFORMATION process{};
        if(!CreateProcessW(loader.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,game.c_str(),&start,&process))return fail(L"Could not start xNVSE.");
        CloseHandle(process.hThread);CloseHandle(process.hProcess);return 0;
    }catch(const fs::filesystem_error&){return fail(L"Could not install the pending plugin update. Close New Vegas and try again.");}
}
