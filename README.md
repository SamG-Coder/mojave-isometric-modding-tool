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

**Left click** walks toward a ground point. **Right click** opens a context menu at the picked object or ground point (and stops the current order). **Mouse wheel** zooms. Hold **middle mouse** and drag horizontally to orbit (yaw), vertically to tilt (pitch); **[ / ]** also rotate. **F8** restores the regular camera and controls owned by the plugin. The pointer and destination ring use the configured HUD colour. The centre crosshair is hidden while the mod owns gameplay.

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

## In-game settings

Open the normal Settings menu from the title screen or the Esc pause menu. Display retains its native live controls (brightness, colours, texture size and fade distances) and adds graphics adapter, resolution, anti-aliasing, anisotropic filtering, fullscreen/windowed mode, VSync, shadows, HDR, depth of field and water options to the same scrolling list. Added graphics settings marked `*` require a restart. Use **Apply display changes (restart required)** at the bottom to queue them; **Cancel unapplied changes** discards edits since the last Apply. Back without Apply also discards the new options when the page is reopened. The existing game's settings keep their normal behavior.

The **Isometric** category is another native Settings entry. It exposes projection, zoom span, pitch, middle-mouse rotation speed, Alt aim line, damage numbers, automatic distant aiming and automatic activation after loading. Use Apply to update these immediately and persist them.

The red cross on the Alt aim line warns of an obstruction but does not prevent ranged attacks. Click to fire anyway once in range; native projectile collision still determines what is hit. Ranged pursuit closes the distance without requiring a clear firing lane. Melee retains its obstruction and reach checks.

The menu uses the game's native list and toggle templates, including its arrows, scrolling, fonts and interface colours. It does not require an overlay menu or replacement menu XML. The options column is widened to accommodate longer graphics values.

Steam Play through MojaveIsoLaunch applies queued display settings before xNVSE starts. Only recognized preference fields are copied into FalloutPrefs.ini. The launcher backs up the original under `backups/Display-*.ini`, preserves unrelated preferences, and keeps pending changes if applying them fails. The menu validates fullscreen resolutions and MSAA support against the selected Direct3D adapter before saving.

Screen picking and indicators use final back-buffer pixels, with explicit mapping from the world-render viewport. Mouse pointer speed and indicator sizes scale with output height; middle-mouse camera rotation keeps its angular sensitivity. Native interaction labels and their click areas share the UI coordinate conversion. Diagnostics now include `world_surface`, `world_viewport` and `display_viewport` for resolution-related reports.

While the orthographic camera is active, light, shadow and specular fade distances expand to cover its ground footprint plus the camera offset and a small margin. Coverage follows zoom, pitch and aspect ratio. This uses the engine's runtime fade caches and does not change FalloutPrefs.ini or force disabled shadow features on. Native distances return when the mod camera is disabled or hands off to the Pip-Boy; in-game Display changes are preserved. A wider lit/shadowed area can increase GPU cost. This does not raise shadow-count limits or load distant world cells.

### Right-click context menu

The context menu uses native HUD tiles, the current HUD font/colour/zoom and a native menu background texture. It sits beside the click and stays inside the screen using native UI coordinates.

- Living actors: Talk/Interact, Attack, Open VATS, Walk here and Cancel.
- Dead actors: Search, Walk here and Cancel.
- Doors and containers: Open / close or Open, followed by movement and cancellation actions.
- Furniture, terminals and activators: Sit / use or Use. The engine selects the actual furniture or scripted interaction.
- Loose inventory objects: Take. Plants: Harvest.
- Ground and scenery: Move/Walk here and Cancel.

Left-click a row to select it. Outside clicks and a second right-click dismiss the menu without sending a world click. Escape also dismisses it while retaining the native pause-menu behavior. Middle-mouse rotation dismisses it; wheel zoom is held while it is open. Native menus, loss of focus, resolution changes, save loads and an unavailable target dismiss it. The world is not paused.

Activation walks into reach and uses the existing native activation flow, retaining locks, ownership, dialogue choices and scripted behavior. Open VATS hands off to the game's regular VATS control; it does not force the selected target or bypass VATS eligibility. The menu exposes supported actions by object type, not an enumeration of arbitrary mod-script commands.

### Experimental RTX Remix: Off / On / Setup

In native Settings > Isometric, select **Experimental RTX Remix**, then Apply. Restart through Steam to change runtime modes. The initial value is Off.

- **Off:** stops catalogue collection and, on next launch, removes this integration's verified entry DLL from the game path. Profiles and captures remain on disk. Other renderer DLLs are never overwritten or removed.
- **Setup:** installs the checksum-pinned NVIDIA Remix 1.5.2 runtime on restart, then records loaded reference IDs, base IDs, cells, positions and camera regions as you play. It requests a single-frame Remix capture after five seconds in a region/view, no more often than every 20 seconds, with at most three attempts per scene and 64 requests per session. Collection runs only in active isometric gameplay. It incrementally indexes the meshes/materials/textures and other files actually exported by Remix. Requests and observed exports are separate records.
- **On:** loads the same saved rtx.conf through DXVK_RTX_CONFIG_FILE and runs the runtime without automatic catalogue collection. The menu requires a catalogue from Setup first. It does not interpret captured USD scenes as replacement mods.

The profile is `runtime/remix/profile/rtx.conf`, with the cumulative catalogue in `runtime/remix/profile/catalog.jsonl`. Actual Remix exports stay in the game's `rtx-remix/captures` folder and are referenced by the catalogue. Capture contents depend on what the runtime recognizes; the native reference catalogue is not a substitute for GPU mesh/material captures. Existing profile configuration edits are retained. Catalogue writes are batched and scene/reference records deduplicated across sessions. `runtime/status.json` exposes requested/session modes and capture status.

This is experimental capture/profile plumbing, not a validated New Vegas RTX remaster: programmable-shader capture, orthographic rendering, lighting/material conversion and native UI compatibility still need live testing and integration. Setup does not automatically author PBR replacement materials or fix incompatible render passes. Native capture indexing observes files; it does not validate the contents of every USD export. No claim is made that all game assets are captured correctly.

For recovery if the experimental renderer prevents reaching the menu, close the game and run `MojaveIsoLaunch.exe --rtx-off`. It persists Off, disables this integration's entry DLL and starts the game. Runtime provisioning failures are recorded in `runtime/remix/error.txt`.

### Experimental independent DLSS 5

Settings > Isometric > Experimental DLSS 5 > On, Apply, then restart through Steam. Keep RTX Remix Off. This enables the experimental native-resolution D3D9On12/helper bridge; it does not provide frame generation or ray tracing. See [DLSS5.md](desktop/DLSS5.md) for supported behavior, dependencies, validation and known limitations.
