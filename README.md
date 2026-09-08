# Mojave Isometric Modding Tool

By **SamGCoder**. A Windows desktop tool and native C++ plugin for experimenting with an isometric, point-and-click Fallout: New Vegas. This is an early working prototype; it is not a complete game conversion.

The native plugin controls engine camera transforms and renderer projection, performs collision picking, steers the player toward a clicked ground point, and captures the active D3D9 render target. The desktop tool provides camera controls, frame previews and diagnostics.

## Requirements and setup

- Fallout: New Vegas runtime **1.4.0.525** (normal runtime ABI). Tested on Steam; other distributions have not been validated.
- [xNVSE](https://github.com/xNVSE/NVSE/releases), installed according to its instructions. Tested with 6.4.8.
- Windows, Visual Studio with Desktop development with C++, a Windows SDK, and CMake 3.24 or newer on PATH. The plugin requires an **x86 / Win32** build and C++20. Tested with Visual Studio 2026.
- Python 3.10 or newer with Tkinter and the Windows Python launcher.

Clone this repository into a directory named **IsometricModdingTool directly inside the game directory**, beside FalloutNV.exe. This location is required by the current plugin and desktop paths:

```text
Fallout New Vegas/
  FalloutNV.exe
  nvse_loader.exe
  IsometricModdingTool/
    native-plugin/
    desktop/
    CMakeLists.txt
```

From IsometricModdingTool, run:

```powershell
py -3 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r desktop/requirements.txt
.\"Build Native.cmd"
.\.venv\Scripts\python.exe desktop/tool.py --install
.\"Launch Tool.cmd"
```

Exit the game before installing or replacing the DLL. The install command copies the built DLL into Data/NVSE/Plugins and backs up a previous copy of this plugin. No game executable, game assets, xNVSE binaries or saves are included in this repository.

## Play and experiment

1. Use **Launch New Vegas + plugin** in the tool, or run nvse_loader.exe from the game directory.
2. Load a character. Isometric mode now enables automatically once the world is ready. **F8** toggles it for the current session.
3. Adjust camera orbit, angle and distance. Enable the experimental orthographic projection if desired.

**Left click** walks toward a ground point. **Right click** stops. **Mouse wheel** zooms. Hold **middle mouse** and drag horizontally to orbit (yaw), vertically to tilt (pitch); **[ / ]** also rotate. **F8** restores the regular camera and controls owned by the plugin. The pointer and destination ring use the configured HUD colour. The centre crosshair is hidden while the mod owns gameplay.

Tool commands are applied when the game resumes processing its main loop after returning focus. Frame previews are snapshots: capture again after moving before clicking a preview to send a destination.

## Project structure

- [native-plugin](native-plugin/README.md): standalone native C++ subproject, camera and renderer hooks, collision picking and movement.
- [desktop/tool.py](desktop/tool.py): Tkinter control panel and command-line client.
- Build Native.cmd: configures and builds the Win32 DLL.
- Disable Native Plugin.ps1: moves this plugin DLL into backups while the game is closed.
- Restore Display Settings.ps1: optional local recovery utility; requires a previously saved backups/FalloutPrefs.original.ini. A fresh clone does not contain that backup or change display settings.
- runtime/, build/ and backups/: generated local data excluded from source control.

## Current limits

Movement steers directly toward a collision hit. Door/NPC/container collision picking and walk-to-activate are implemented as experimental groundwork and need further validation. Navmesh routing around obstacles, combat conversion, and roof/wall cutaways are not implemented. Interiors, dialogue, scripted cameras, VATS, distant terrain culling and compatibility with other camera mods need more testing. The renderer is hooked; it is not a replacement renderer or a full renderer editor.

See [VERIFICATION.md](VERIFICATION.md) for observed in-game results and the boundaries of testing.

## Licence and credits

Copyright (c) 2026 **SamGCoder**, for original contributions. This project is distributed under the **GNU General Public License version 3.0 (GPL-3.0)**; see [LICENSE](LICENSE). Upstream copyright and licence notices remain applicable to incorporated or adapted material; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

This is an unofficial modding project, unaffiliated with Bethesda or Obsidian. Fallout: New Vegas and its game assets belong to their respective owners.

## Start directly from Steam

After building and installing, the installer also copies MojaveIsoLaunch.exe to the project root. In Steam Properties / General / Launch Options, use the absolute path to that executable followed by `%command%`, for example:

```text
"D:\SteamLibrary\steamapps\common\Fallout New Vegas\IsometricModdingTool\MojaveIsoLaunch.exe" %command%
```

Steam Play then launches xNVSE directly. The desktop tool is optional. Settings persist in runtime/settings.ini. The launcher installs a staged runtime/MojaveIsoNative.pending.dll before starting the game; a running game must be restarted to load an updated plugin. Clear the Steam launch option to restore the original launcher.
