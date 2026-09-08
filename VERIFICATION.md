# Mojave Isometric Modding Tool — verification

Tested on 9 September 2026 against the installed Steam FalloutNV.exe, file version 1.4.0.525.

## Confirmed

- Native Win32 C++ DLL built successfully with MSVC 19.51 / Visual Studio 2026 and the Windows SDK.
- xNVSE 6.4.8 loaded `MojaveIsoNative.dll` and registered its message listener.
- Both pairs of camera hook call sites passed runtime compatibility checks.
- Direct camera transform overrides produced an elevated, player-following view in Goodsprings.
- NiDX9Renderer::SetupCamera hook executed; diagnostics reported `rendered_orthographic: true`, not merely an enabled configuration setting.
- Collision picking produced a world-space destination from supplied screen coordinates.
- Walking under orthographic projection reached a target at approximately (-71613.4, 235.683, 8246.85), stopping at (-71624.1, 244.315, 8250.13), within the 24-unit horizontal arrival tolerance. The plugin reported `Destination reached` and released movement.
- Another destination test also completed before the renderer projection hook was added.
- Physical F8 input switched back to the normal third-person camera; the resulting frame was visually inspected.
- Active D3D9 render-target capture, including multisample resolve, produced a correct in-game image and a correct main-menu image.
- The desktop application launched; its frame preview, camera panel, scrollable controls and diagnostic layout were visually inspected.
- Python source passed compilation checks.

## Not established by these tests

- Routing around obstacles: absent. The current controller steers in a straight line and has an obstruction timeout.
- Mouse interaction with doors, items or NPCs: absent.
- Roof/wall cutaways and material editing: absent.
- Combat conversion or turn-based combat: absent.
- Full campaign, interiors, VATS, dialogue, scripted cameras and other camera-mod compatibility: not validated.
- Complete culling correctness with orthographic projection over large distances: not validated.

The native plugin modifies the running engine, not FalloutNV.exe on disk. The renderer integration controls camera submission and reads the current render target; it is not a replacement game engine or full renderer editor.

## Wheel isolation regression (native version 0.4)

The mouse GetDeviceState hook now removes wheel deltas from the result sent to the original game camera while isometric mode owns input. The existing device implementation, including xNVSE processing, is called first. Keyboard devices and menu/normal-camera wheel input pass through. Queued zoom is reset on menu handoff, enable/disable and pre-load.

Validated: Release DLL and wheel-test executable build; wheel ownership, signed magnitude, multiple-poll accumulation, single consumption, menu passthrough and load-reset checks pass. Version 0.4 and its mouse hook loaded in New Vegas. Increasing span to 3000 and camera distance to 2200, then reloading the dedicated test save, retained those settings and resumed confirmed orthographic rendering. Previous camera settings were restored afterward.

The reported physical mouse-wheel to first-person threshold sequence still needs a human playtest; the automated runtime check changed zoom through the plugin command interface. No vanilla first-person threshold setting was edited.
