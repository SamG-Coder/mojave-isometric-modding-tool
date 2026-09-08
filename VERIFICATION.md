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

## Camera and pointer regression (native version 0.5)

Committed input changes isolate mouse X/Y deltas as well as wheel input from vanilla look. Middle drag changes yaw and pitch with smoothing and pitch limits. Pick rays use the last displayed renderer camera; destination markers use the current renderer camera. Ground validation retains the original hit point. Pointer/marker colour follows uHUDColor. Reticle image visibility is temporarily hidden and restored when releasing gameplay ownership.

Validated: Release build, 1,176 pixel-to-world-to-pixel checks across pitch/yaw, orthographic/perspective and nonzero viewport offsets, and existing wheel regression checks. The final installed DLL hash matches the built DLL. Captured in-game imagery shows the amber marker/pointer and no centre crosshair. Live click diagnostics reported reprojection errors of 0.000533 and 0.003386 pixels. Live camera diagnostics showed pitch 30 degrees with changed yaw. This measures projection alignment; collision geometry can still differ from visible meshes, and obstacle navigation remains a separate limitation.


## Version 0.6 correction status

The initial fixed action list and collision-grid minimap were rejected during playtesting and have been removed from the active HUD. A native TileText prompt now loads through the engine UI system and copies the vanilla activation-text font and colours. It is shown only for a hovered reference within 350 units, with a 150-unit height limit and matching parent cell. Native tile creation was confirmed in the live log; final prompt positioning and clicking still need visual validation.

The experimental BSCullingProcess plane override and per-geometry stencil cutaway are disabled after missing geometry was reported. The orthographic plane mathematics pass corner and outside-plane tests, but this does not establish integration correctness. Missing player geometry also appeared in a capture after these hooks were disabled. Read-only inspection confirmed third-person mode and an existing FaceGenFace mesh with its hidden bit clear. Root cause remains unresolved; do not label culling or cutaway complete.

A* route planning passed deterministic obstacle, step-height, unreachable-target and cancellation checks. Live logs reported routes reaching destinations. The corrected form-table lookup activated Prospector Saloon reference 0010636F; a subsequent frame showed dialogue with Sunny Smiles inside the saloon. Full interaction regression coverage is still outstanding.

The native Pip-Boy local-map rendering integration is not implemented. No debug map is presented as a substitute.


## World-transform culling correction (staged)

Renderer submission, the camera frustum and the culling adapter now use one worldFrustum definition. The culling adapter calls the engine's NiFrustumPlanes builder at A74E10 with the final NiCamera world transform at +68. The engine implementation was inspected in live process memory and explicitly handles the orthographic flag. The adapter temporarily excludes the BSCullingProcess compound volume for this camera, retaining the ordinary world-bound sphere test at A694E0; other camera passes are unchanged. Original per-process state is restored after traversal.

This supersedes the disabled hand-built plane override. The cutaway geometry hooks remain disabled. Release build and 1,176 camera round trips pass, with added partial-sphere intersection, tangent, fully-outside and camera-volume corner cases. These mathematical tests do not prove live visibility. DLL staged for the next launcher start; saloon foreground-object visual regression and performance assessment pending. Excluding compound occlusion can increase draw calls, particularly indoors.

User playtest confirmation: the world-transform culling correction fixes the reported foreground-object disappearance. Confirmed in conversation after installing the staged build.
