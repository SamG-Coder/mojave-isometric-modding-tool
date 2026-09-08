// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder (original contributions).
// See THIRD_PARTY_NOTICES.md for upstream attribution.
#pragma once
#include <cstdint>
// Minimal public xNVSE ABI, checked against xNVSE/NVSE PluginAPI.h, 6.4.8.
// No engine classes from the older SDK are assumed: offsets live in engine.hpp.
struct NVSEInterface {
    uint32_t nvseVersion, runtimeVersion, editorVersion, isEditor;
    void* RegisterCommand; void* SetOpcodeBase;
    void* (*QueryInterface)(uint32_t);
    uint32_t (*GetPluginHandle)();
    void* RegisterTypedCommand;
    const char* (*GetRuntimeDirectory)();
    uint32_t isNogore;
};
struct PluginInfo { uint32_t infoVersion; const char* name; uint32_t version; };
struct Message { const char* sender; uint32_t type, dataLen; void* data; };
struct Messaging {
    uint32_t version;
    bool (*RegisterListener)(uint32_t, const char*, void (*)(Message*));
    void* Dispatch;
};
struct Console {
    uint32_t version;
    bool (*RunScriptLine)(const char*, void*);
    bool (*RunScriptLine2)(const char*, void*, bool);
};
struct ScriptResult { union { double number; void* pointer; }; uint8_t type; };
struct Scripts {
    bool (*CallFunction)(void*, void*, void*, ScriptResult*, uint8_t, ...);
    void* GetFunctionParams; void* ExtractArgsEx; void* ExtractFormatStringArgs;
    bool (*CallFunctionAlt)(void*, void*, uint8_t, ...);
    void* (*CompileScript)(const char*);
    void* (*CompileExpression)(const char*);
};
static_assert(sizeof(void*) == 4);
static_assert(sizeof(ScriptResult) == 16);
