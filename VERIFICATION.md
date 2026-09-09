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

## Chase-to-attack responsiveness

Weapon readiness is now requested during pursuit instead of first being requested upon reaching attack range. Combat range/line-of-fire decisions run before the walking planner each update, cancelling movement and its pending search when an attack opportunity exists. Native weapon readiness, firing cadence, reload and animation gates still apply.

Pursuit refreshes are bounded to 250 ms and compare the actor's current position with its position at the previous plan, using a 32–128 unit movement threshold scaled to weapon range. A substantially moved target can replace a stale pending search; stationary targets do not repeatedly restart it. Approach samples always try the player's nearest side first, then alternate sides, instead of rotating the starting sample after each successful plan.

Release plugin build and combat/follower regression tests pass. Added pursuit tests cover immediate initial planning, update throttling, a moved target during a pending search, small movements, exhausted routes and unchanged pending searches. This corrects identified scheduling/readiness delays but is not live verification of the user's specific moving-target encounter. Build staged for next launcher restart because the game is running.

## Native mesh seam connectivity correction

Replaced geometric edge matching with explicit native triangle adjacency. Runtime triangle flag bits 0–2 select EdgeExtraInfo indices for their respective sides; those records identify the destination mesh and triangle. Previously those indices were treated as local triangle indices, while cross-mesh links were inferred from a mesh pair plus edge coordinates quantized to 0.01 units. This could drop legitimate native seams and force long alternate routes.

The snapshot now resolves each external edge's destination. The graph joins reciprocal native edge references without requiring coordinate equality. Missing/disabled/unloaded destination triangles remain excluded. Portal search costs and reconstruction include both sides of a seam, so slightly separated edges remain explicit route segments subject to follower collision checks. Straight-line shortcuts still require connected triangle coverage; no proximity-based links are invented.

Flag semantics verified against TES5Edit Core/wbDefinitionsCommon.pas (wbNavmeshTriangleFlags and wbNVTREdgeToStr), dev-4.1.6: https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsCommon.pas . Runtime EdgeExtraInfo layout verified against the local JIP/JohnnyGuitar source headers. Regression tests pass for a non-identical-coordinate seam, incorrect target triangle, missing reciprocal link, disconnected coincident geometry, short tessellation edges and both long/short obstacle detours. Large synthetic detour: 12,660 triangles, 27,374 entry states, 5.5185 ms including graph construction. Release build and path/follower/combat tests pass. The user's specific reported boundary is not yet live-verified.

## Furniture, dialogue camera and combat modes

Furniture base type 0x27 is now accepted by interaction picking and native activation. While the player is seated (Actor sitSleepState +1AC), gameplay input ownership is released so vanilla furniture controls can operate, while the isometric camera remains active. Furniture activation is subject to native occupancy/script restrictions. Furniture acceptance and sitting behavior have not yet been confirmed in live play.

Camera ownership is separate from gameplay input ownership. Dialogue retains the isometric camera and orthographic culling while dialogue menus retain native input. Live process 8280 loaded dedicated MojaveIsoLabTest, entered the saloon, and opened Sunny Smiles dialogue: dialogue=true, camera_blend=1, controls_owned=false, rendered_orthographic=true. Captured frame visually confirms the isometric interior and native dialogue choices.

Melee target distance now uses actor feet (horizontal distance with vertical separation guard), not an animated projectile node. Melee clearance originates at body height, and attacks use 70 ms native input pulses with a 500 ms retry interval instead of single-frame taps. These are normal attack requests, subject to native animation, stamina/AP and damage handling; melee damage is not yet live-verified.

Distant non-melee attacks request native aim control 6. Aim enters above 500 world units and exits below 400 to avoid rapid switching. Close ranged attacks hip-fire; melee never requests aim/block. Script-only permissions are opened for the owned aim binding, matching attack input ownership. Aim is released on pursuit, cancellation, single-shot completion and menu handoff. First aim activation is processed before firing on the following update. The renderer retains isometric camera span/projection, and aim geometry continues to use the weapon projectile origin and target torso. Native aim acceptance/spread and long-distance hit accuracy remain to be validated in-game; no perfect-hit or trajectory override is implemented.

Release builds pass; combat tests cover melee distance/floor separation, aiming thresholds/hysteresis, dialogue/seated camera ownership, unrelated menus and camera disable. Existing camera and follower tests pass. The updated DLL is installed and running in the dedicated test session. Changes are uncommitted.

## Persistent camera ownership

Camera ownership now depends on isometric mode being enabled with a loaded player world, rather than vanilla gameplay/menu/third-person flags. Native POV changes and menu handoffs no longer blend back to the native camera. Position and rotation hooks reassert the complete isometric transform, recomputing the basis independently in both hooks, without blending in incoming native rotation. Ordinary gameplay also restores third-person mode if vanilla changes it while isometric remains enabled; dialogue and furniture animation state are excluded from that POV correction. Gameplay input ownership remains separate, so native menus and furniture controls still receive input.

Explicit mod disable and world unload release ownership. Release build, ownership tests and 1,176 camera projection round trips pass. This update is installed while the game is closed; it has not yet been live-tested against the user's latest camera takeover. It does not establish coverage of every cinematic camera path that may bypass the existing player-camera hooks.

## Native flat Pip-Boy menu

The previous persistent POV correction also ran during Pip-Boy opening. It now excludes every nonzero InterfaceManager pipBoyMode (+4BC), and isometric gameplay input releases ownership through opening/open/closing states. Camera ownership remains independent.

While enabled, the plugin temporarily sets native bUsePipboyMode:Pipboy false (runtime setting 11DB2CC +4), restoring the captured value on disable. This selects the engine's existing screen-space menu renderer rather than requiring the wrist model/world camera. No replacement inventory, rendered texture copy or custom Pip-Boy content is introduced. Native menu roots for Inventory, Stats and Map are centered in the native UI coordinate space, with positions restored on closure/disable while the same root remains alive. Runtime settings are not written to the user's INI.

Live process 18840: native TapControl 14 opened Pip-Boy (mode 3) while isometric camera_blend remained 1; before selecting flat mode the UI was invisible. Applying native SetINISetting bUsePipboyMode:Pipboy 0 made the actual Stats screen visible over the world in a captured frame. This verifies the native rendering switch. Full mouse interaction, all tabs, repeated open/close cycles and final automatic centering are not yet live-verified. The production integration builds without warnings and is staged for restart; existing camera/ownership regression tests pass.

## Full native Pip-Boy with hidden arm (supersedes flat-menu experiment)

User requested the complete Pip-Boy model and its close-up camera instead of the flat transparent page. Removed the flat-page centering adapter and now select native wrist-model rendering while isometric mode is enabled, preserving/restoring the previous setting on disable. Pip-Boy opening/open/closing states explicitly release the isometric transform and projection hooks so its native close-up camera, screen, physical page buttons and cursor operate together. Isometric angle/span settings are retained for return to gameplay. The standalone flat-page drawing mode is no longer active, addressing that source of menu overlap; ESC stacking is not yet live-confirmed.

Only the first-person upper-body/left-hand/right-hand biped slot models are temporarily hidden while Pip-Boy is active. The skeleton and Pip-Boy slot are explicitly excluded. Each slot's prior hidden bit is restored when it closes or the mod is disabled, checking the current slot model identity before restoration.

Live process 22640, dedicated MojaveIsoLabTest: native TapControl 14 opened mode 3 with camera_blend=0. Captured images show the full device, Stats/Items/Data physical buttons, native Stats page and visible cursor, with no surrounding hand/arm. Cursor position changed between captures. This is visual verification, not proof of every page action or ESC stacking. Test input did not confirm the close transition, so repeated open/close still needs checking. Release build, camera ownership tests (all nonzero Pip-Boy modes release ownership, zero restores it), 1,176 projection checks and wheel/menu handoff tests pass. Built DLL matches installed DLL in the running test session. Changes remain uncommitted.

## Single-shot aiming lifetime correction

Non-actor shots previously cleared AttackOrder immediately after TapControl, allowing the next update to restore prior pitch or follow a moved Alt cursor before the native firing animation/projectile release. Single shots now retain the clicked point and sights through an observed native attack action (BaseProcess::GetCurrentAction virtual slot 3E4, actions 2–5), with a 150 ms minimum after submission and a 1.5 second timeout if the game does not produce/finish the action. This does not prove that a shot was accepted. Explicit cancellation and menu handoff still release ownership immediately. No duplicate shot is submitted while waiting.

An 80 ms alignment window precedes the first ranged attack after acquisition/readiness, allowing animated muzzle transforms to respond to actor rotation. The clicked non-actor reference is retained for line-of-fire checks, so the target object itself is accepted as the first collision. Single-shot diagnostics record requested world point, muzzle position and actor angles; these are input diagnostics, not measured projectile trajectories. Spread and game hit logic remain native.

Release build and tests pass for retaining aim on the next frame and during native action, action completion, delayed/rejected inputs and bounded timeout. Existing camera/follower checks pass. The bottle quest has not yet been reproduced and successful bottle hits are not live-verified. Do not attribute all previous misses to this timing bug without live projectile evidence.

## Floating player damage numbers

Character and Creature DoHealthDamage virtual slots (+4B8, runtime tables 1086A6C/10870AC) are wrapped without changing their arguments or results. When the source is the player, the wrapper samples current Health via ActorValueOwner (+A4, GetActorValue slot 3, AV 10) immediately before and after the native call. The resulting health loss is clamped to remaining positive health, ignoring healing, unchanged/invalid values and losses below 0.05. Other attackers and player self-damage are excluded. Damage routes that bypass these native methods or omit player source attribution are not covered.

A bounded queue of 24 popups stores world positions rather than retaining actor pointers. Native TileText uses the HUD activation font and colours, projects above the target and rises/fades over 1.4 seconds. Menus suppress display; old popups expire and load events clear them. No UI hit targets are added. Asset: native-plugin/ui/damage.xml, deployed to Data/menus/MojaveIso/damage.xml (install this alongside interaction.xml on a fresh installation).

Release DLL build and dedicated damage tests pass for ordinary damage, capped overkill, already-dead targets, healing, unchanged/invalid health and fade lifetime. The DLL and XML are installed while the game is closed. Live damage callback and visual popup behavior are not yet verified; do not treat calculation tests as a successful in-game hit demonstration.

## Alt aim diagnostic line

While Alt aiming, a HUD-coloured line projects from the animated weapon muzzle (head fallback) along the actor yaw/pitch used by combat. A main-thread collision ray truncates it at the first hit; an early obstruction has a red endpoint cross and the intended point has a faint cross. Menus, camera dragging and stale aim suppress the line. This is a diagnostic overlay, not a physical laser or measured projectile trajectory; native spread remains in effect. Both endpoints must project into the viewport.

Release build and combat direction round-trip tests pass. Live visual placement and collision behaviour are not yet verified.

## Baseline xNVSE held input and native aiming acknowledgement

Root cause found in source and installed plugin inventory: HoldControl/ReleaseControl are JIP commands, while this installation contains only MojaveIsoNative.dll. Replaced those calls with baseline xNVSE HoldKey/ReleaseKey using configured keyboard and mouse bindings. Captured bindings are retained until release, including cancellation and menu handoff. This also repairs the same missing-command path for automatic fire and melee pulses. No JIP installation is required.

Alt hover now requests distance-based aiming for ranged weapons. Distant attack submission checks BaseProcess::GetIsAiming (virtual slot 404, verified against local JIP and xNVSE headers) continuously for 120 ms before the existing muzzle alignment interval. If the engine does not acknowledge aiming, the attack waits and status reports that reason rather than submitting hip fire. Aiming status distinguishes requested input from actual native aim. This is an aiming-state check, not a direct measurement of projectile spread or a guarantee of iron-sight animation completion. Camera ownership is unchanged.

Release build and combat tests pass, including rejected aim input, observed-state settling, loss/reacquisition and close-range hip fire. Runtime input/aiming and hit accuracy still require in-game verification.

## Projectile launch direction and native camera convergence

Live status from the user's last session reported aim_down_sights_requested=true and aim_down_sights_active=true, so the persistent misses were not explained by failure to enter native aiming. Inspected the unpacked 1.4.0.525 runtime firing path. Weapon fire at 5245BD calls projectile creation 9BCA60 with origin, heading/pitch and separate native random spread. Projectile initialization subsequently calls player convergence 965620 from 9BD9E2, passing projectile rotation/position by pointer; that routine reads native camera coordinates at 11F426C.

A guarded launch-call wrapper now computes direction from the actual launch position to the ordered world point and adds the native per-projectile spread. A thread-local scope suppresses only the later camera-convergence call during these corrected launches. Both call targets are validated before installation. Scope: active isometric player orders, weapon types 3–8, and non-forced-hit launch arguments. Other launches pass through. Position, weapon, ammo, damage, spread arguments and native projectile creation remain unchanged. No post-spawn projectile steering is used.

Live evidence, diagnostic varmint rifle: initial launch-only correction sent yaw/pitch 3.11667/0.256097, but creation returned a projectile at 2.94804/0.432341, roughly ten degrees off each axis. After suppressing the scoped convergence step, process 11996 returned 3.11614/0.255497, exactly matching the corrected input. A second direction returned 2.34405/0.283479, also matching. Native spread offsets were retained and nonzero. Release build and combat tests pass; running installed DLL SHA256 matches build. This verifies projectile initialization direction, not the bottle quest completion or every weapon type.

Testing used MojaveIsoLabTest and a new dedicated MojaveIsoAimDiagnostic save with a test rifle/ammunition; no user save slot was overwritten. Projectile launch diagnostics record original/corrected angles, spread, launch origin, ordered target and resulting projectile rotation. Native collision/damage and all alternate weapons still need gameplay coverage.
