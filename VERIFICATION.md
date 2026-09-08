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


## Indoor pathfinding and roof picking follow-up

Searches now cache successful and failed floor samples and directional connections, discard caches for each new route, distinguish floor-height layers, and use a 32-unit grid. Each update yields between expansions after a 2 ms budget or 12 expansions (an individual expansion can exceed 2 ms). Walking obstacle probes are limited to ten per second; newly blocked routes trigger replanning. Long connections check supporting floor; arrival on another floor is rejected for ground destinations.

Interior/covered-player screen rays begin at player-floor height +100 while retaining their original line, to avoid selecting overhead roof geometry. This deliberately prioritizes the current floor; selecting upper floors remotely is not supported by this policy. It does not hide roofs visually.

Tests pass for a narrow doorway, cache reuse/reset, blocked routes, height changes, roof-ray alignment, projection round trips and input ownership. Synthetic doorway case: 13 expansions, 134 collision callbacks, 233 reused results. The live build entered the saloon and reported interior route queries/cache hits and negligible pick reprojection error. This is not an FPS benchmark or exhaustive indoor-layout validation. The final different-floor arrival correction is staged for next launch.


## Click-to-walk latency follow-up

A budgeted direct-corridor phase now precedes A*. It samples floor support and body clearance in 32-unit segments, falls back to A* on obstruction, and never starts unvalidated movement. A* equal-score ties prefer the candidate with less remaining distance. Status now reports planning_ms (the last completed search duration) and direct_route.

Tests: an unobstructed 256-unit route completes in one 12-segment update with zero A* expansions; obstacle detours, floor gaps, doorway clearance, cancellation and height tests pass. Doorway case uses 12 expansions, 121 collision callbacks and 220 cache hits. These are synthetic checks, not measured live click-to-walk latency. The updated DLL is staged for next launch.


## Combat foundation (staged, not live-verified)

Release build passes. Unit checks cover world-space yaw/pitch and attack gating for range, obstruction, movement, menu state and dead targets. Navigation and input regressions pass. Attack requests use mapped native input, allowing the game to handle firing animations and ammunition. Physical attack remains disabled while script input is enabled only for the attack control owned by this plugin.

Live projectile direction, ammo consumption, sustained fire, melee reach, approach routes, cancellation and VATS handoff still require playtesting after the DLL reload. Native VATS target preselection is not implemented. Do not treat compile/unit success as proof of working combat.


## Continuous route following and replacement

The active Follower route is now separate from the Search being calculated. Retargeting preserves walking input and the existing route; replacements must join from the current player position through a freshly checked corridor before installation. Nearby goals can replace a short route directly or extend its end. Shortcuts are checked before skipping waypoints. A 350 ms look-ahead detects changes beyond the immediate segment and starts a replacement while movement continues. Immediate obstruction, exhaustion of the safe path, explicit cancellation or unrecoverable stalls still stop movement. Stalls receive up to three repair attempts. Search caches remain per-search; stale cross-search collision results are not reused.

Release build and follower tests pass: active-route retention during a pending search, progress during planning, blocked-join retention, current-position adoption, local adjustment, blocked-shortcut rejection and explicit cancellation. Existing path, combat, camera and wheel regressions pass. Live movement continuity remains to be checked after restart. Combat baseline committed locally as 652c47c; this follow-up is uncommitted.

## Aim-origin correction
The aiming and line-of-fire calculation now uses the equipped weapon projectile node when available, with the animated player head as fallback. Humanoid targets use Bip01 Spine2 instead of the overall scene-bound centre. Tests cover lateral muzzle offsets and lower/crouched target positions. Release build passes; actual projectile hits remain unverified. The user reports shots appear to travel toward targets but fail to hit. No spread, damage, hit detection or projectile trajectory has been overridden. This follow-up remains uncommitted.

## Faster A* implementation
Ordered maps for floor samples, directional connections and node IDs are replaced by hash tables. Node and heap vector capacity is reused between searches, hash tables are pre-sized, and collision callbacks are templated rather than converted to std::function. Search costs, heuristic, clearance rules and expansion order are unchanged.
Release benchmark with 60 identical two-wall detours: ordered-map baseline 87.1732 ms; revised implementation 41.8323 ms (2.08387x). Both found 60 routes, expanded 27,780 nodes and made 189,720 callbacks. This benchmark uses lightweight collision stubs and measures planner overhead, not game physics or end-to-end movement latency. Pathfinding and follower regressions pass. Runtime diagnostics now separate planner CPU milliseconds, update count and elapsed planning time; failed searches update elapsed timing rather than displaying a stale successful duration. DLL staged; live latency comparison pending.

## Native navigation mesh routing — installed and live-tested

New movement orders now first copy the loaded native NavMesh triangles and search their adjacency graph. Disabled triangles/meshes and unloaded neighboring cells are excluded. Same-mesh connections require reciprocal native side indices; cross-mesh connections require a native mesh link and matching shared edge. Portals narrower than 40 units are rejected. Portal-midpoint routes are densified for the existing follower, which checks the initial join and upcoming segments against live collision. No physics-sampled grid is needed for the native graph search itself.

An unavailable native route or blocked initial join falls back to the existing grid planner. Dynamic-obstacle repairs still use that grid planner, preserving the existing safe route where possible. This change does not implement teleport-door graph traversal, asynchronous planning, a funnel smoother, or a persistent mesh cache. Native graph construction/search runs synchronously per new order and is bounded by the adapter's mesh/triangle limits; no universal latency guarantee is implied. Off-mesh destinations and dynamic obstacles can still produce slower fallback searches.

Live process 19052, dedicated MojaveIsoLabTest save: an outdoor destination completed with navigation_source=navmesh, 8,832 loaded triangles, 2 expansions, and 6.8103 ms native planning time. A longer outdoor destination completed with the same triangle count, 7 expansions, and 8.825 ms. Both reported Destination reached with zero grid ground/edge query counters. After teleporting beside the known saloon door for an interaction regression, door activation entered the interior and triggered Sunny's dialogue without a crash; the short approach used a 1-expansion native route (6.6926 ms). This does not validate complicated indoor walking. Native timing covers snapshot, graph build and search; it excludes click dispatch, follower adoption collision checks and travel time.

Synthetic obstacle stress test: 12,660 triangles with a long missing strip, 10,211 expansions, successful detour in 8.8157 ms including graph build. This is a synthetic Release result, not an in-game complex-path benchmark. Tests also reject disconnected touching triangles, unlinked mesh seams, wrong-floor starts, off-mesh endpoints and missing regions. All six test executables pass: navmesh, path, follower, combat, camera and wheel. Built and installed plugin SHA256 both equal 8DFEAC17CB072278406F9DFCD5F8C2D5D24F05AFE299288DAA4145F064CDD129. This installed live build supersedes the earlier staged-only notes for the A* optimization and aim-origin changes. User subsequently confirmed combat behaved as intended; low skill/VATS hit chance explained the reported misses.

## Route length correction

The native planner now checks for a straight route across connected triangles before A*. Obstacle searches use distinct entry-edge states and costs between portal positions, including the actual start and goal, instead of triangle-centre distances. A bounded-lookahead shortening pass removes unnecessary midpoint detours using the same adjacency-constrained segment checks. Surface crossings retain height samples and large slope changes reject shortcuts. This is not a guarantee of the globally shortest continuous path: obstacle corridor selection still uses portal midpoints as representative positions.

Removed the blanket rejection of shared edges shorter than 40 units: edge length alone cannot distinguish small tessellation in open space from a narrow physical doorway. Native connectivity and live follower collision checks remain in force. Route reuse also rejects an old path whose remaining length exceeds 1.35 times direct distance plus 64 units, allowing a fresh route calculation while retaining the old safe route during fallback search.

Release tests pass for an off-centre clear route matching straight-line distance, small connected triangles, disconnected regions, floor separation, blocked shortcuts and an obstacle with two openings where the near opening must win. The 12,660-triangle large detour test passes at 9.8296 ms including graph build, with 27,373 entry states expanded; this expansion count is not comparable to the previous triangle-state count. Follower tests preserve efficient reuse but reject winding old routes. Path regression tests pass. These are synthetic checks; the user's specific in-game detour has not been reproduced or confirmed fixed by playtesting.
