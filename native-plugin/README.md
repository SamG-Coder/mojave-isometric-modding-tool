# MojaveIsoNative

Native Win32 C++ subproject for the Mojave Isometric Modding Tool. The DLL is loaded by xNVSE; its camera, renderer, collision-picking and player-view integration call the engine directly. JIP and JohnnyGuitar are research references, not runtime dependencies.

## Layout

- `src/plugin.cpp`: plugin lifecycle, scoped engine camera hooks, renderer projection hook, D3D9 capture, input, command polling and diagnostics.
- `src/engine.hpp`: version-specific addresses, scene transforms, Havok collision query and actor movement adapter.
- `src/nvse_abi.hpp`: minimal public xNVSE loader/messaging/console ABI.
- `CMakeLists.txt`: standalone x86 DLL target, also included by the parent CMake project.

## Build independently

Use Visual Studio with its C++ desktop workload, CMake and a Windows SDK (tested with Visual Studio 2026):

```powershell
cmake -S native-plugin -B build-native -A Win32
cmake --build build-native --config Release
```

The parent project's `Build Native.cmd` builds `build/native-plugin/Release/MojaveIsoNative.dll`, which is the location used by the desktop tool's installer. Exit the game before replacing its loaded DLL.

## Integration points

Supported runtime is exclusively FalloutNV.exe 1.4.0.525, normal Steam/GOG runtime ABI `0x040020D0`. Testing was performed on Steam. Other executables are rejected at query time.

The camera transform hook checks that both engine call sites are `CALL rel32` instructions targeting the same in-executable setter before patching. The renderer hook intercepts NiDX9Renderer::SetupCamera at vtable offset `0x18C`; projection changes apply only to the world camera at our controlled position, not UI or shadow cameras. Hook checks are compatibility checks, not proof that every mod combination is supported.

The renderer frame is obtained from the active render target and resolved out of multisample antialiasing before CPU readback. No D3D9 proxy DLL is installed. Existing game binaries and asset archives are not patched on disk.

The main-thread command interface reads `../runtime/command.ini`; atomic replacement and a monotonically changing sequence distinguish requests. Commands already on disk are ignored when the plugin loads. The plugin emits `status.json`, `native.log`, and requested `frame.bmp` captures. No TCP listener, arbitrary memory-write endpoint, or elevated process is used.

Operations: configure, enable, disable, move (screen coordinates), stop, capture, console (Fallout console syntax only). The desktop app and its CLI write the same protocol. Renderer callbacks own rendering; world mutations run on the game's main thread.

## Boundaries

Click walking steers toward a collision hit using the normal player input path. It is not navmesh route finding. Arrival tolerance is 24 game units, obstruction timeout 1.8 seconds, and travel timeout 30 seconds. Elevated hits are rejected. Right click, menus, focus-loss detection and F8 cancel movement. Mouse attack and aim are suppressed while the mode owns them; previous disabled-control state is respected.

Scene cutaways, roof hiding, navigation around obstacles, mouse-driven object activation and combat conversion are not implemented. Orthographic culling, distant terrain, interiors, dialogue, VATS and scripted cameras need further compatibility testing. The renderer is hooked, not replaced; shader/material authoring and a full render-graph inspector are future work.

## Source references and licensing

This project is provided under GPL-3.0; see the parent `LICENSE`. Engine layout and collision-query work derives from research in jazzisparis/JIP-LN-NVSE (GPL-3.0). Camera call-site and ActorMover research also used carxt/JohnnyGuitarNVSE (LGPL-2.1; license included here). The xNVSE public plugin interface is the loader boundary. Source repositories:

- https://github.com/jazzisparis/JIP-LN-NVSE
- https://github.com/carxt/JohnnyGuitarNVSE
- https://github.com/xNVSE/NVSE

No Bethesda assets or original game executable are included in the source package.
