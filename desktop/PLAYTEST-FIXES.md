# Playtest fixes: pickup, dialogue and melee

## Seated mouse movement and attack input

The native cursor, cursor deltas and picking projection now remain available
when the player is seated but gameplay movement is allowed. Clicking ground
submits the native forward-control tap to leave furniture, retains the chosen
destination through the standing transition and starts pathfinding afterward.
Scripted movement locks and dialogue still prevent new walking orders. Right
click, menu/focus changes or an eight-second timeout cancel the pending walk.

Physical fire/aim mouse buttons are filtered at DirectInput while the camera
owns gameplay input, including pending Search activation. Attack controls are
not released merely because activation is pending. Native menu mouse input and
scripted attack input remain separate.

Weapon range no longer vetoes ranged shots or initiates ranged pursuit. Native
projectile collision, ammunition, reload and firing animation still apply.
Melee reach and obstruction checks remain. Local combat, context and wheel
tests passed; the seated cursor and weapon/Search sequence need an in-game
check by the user or a separately authorized playtest.

- Native third-person detection now uses PlayerCharacter::is3rdPerson at 0x64A (JIP-LN-NVSE GameObjects.h), rather than the unrelated byte at 0x64C.
- Walking uses the held native forward binding. It no longer overwrites native movement flags every frame; stopping clears directional flags through the documented mover method.
- Melee pursuit waits for native attack/recovery actions before chasing again, and allows the submitted attack press its full input window.
- Small pickup and harvest objects have an 18-pixel selection tolerance at 720p, scaled with resolution. Candidates are cached for 800 ms.
- Right-click > Pickup Area opens a native Tile/font/HUD-colour list with four items per page, Previous/Next, Take all and Close. This is a native-styled world-item list, not the actual ContainerMenu or a temporary inventory container.
- The list searches 250 game units around the clicked point, limited to 600 units from the player, the same cell/floor, and unobstructed items. It includes scripted Pick/Harvest activators such as Xander Root. The English activation prompt is used to identify scripted plants; translated/custom prompts may require an additional classifier.
- Taking invokes the original reference activation after walking into reach, retaining native scripts, item state and theft handling. No items are cloned or moved into a temporary container. Right-click or a new move/attack cancels the collection queue. Items remain in the world until actually activated.
- Ordinary pickups no longer fade to black. Dialogue/door transitions retain their existing fade behaviour.

Build and automated combat, context and route-follower checks passed. In-game validation remains: exit dialogue without Tab; pick Xander Root; collect loose items via Pickup Area; chase and repeatedly attack a moving enemy with a machete. DLSS/Remix remain parked.

Dialogue visibility is checked during conversations and for 750 ms after closing. Scripted startup retains the native camera until the player is out of furniture, dialogue has ended, and movement is enabled. These latest changes still need live verification.

## Opening character creation guard

The fresh-game playtest reproduced premature isometric takeover while Doc Mitchell was speaking before appearance selection. His scripted SayTo lines can run without DialogueMenu, and the old IsControlDisabled query checked xNVSE input state rather than the native DisablePlayerControls flags.

The opening now retains native first-person/cinematic camera ownership through VCG01 stage 54. Stage 55 is the game's own appearance-complete, standing-complete movement handoff. The plugin additionally requires native movement availability, no furniture animation, and no dialogue/menu before switching to third person/isometric. Intro saves reconstruct this guard on load. This reads quest state; it does not advance the quest or change its scripts.

Mod movement, context actions, pickup, attacks, aim indicators and camera toggling cannot bypass the opening guard. Native movement and combat restrictions remain respected after the initial handoff, including subsequent scripted tutorial dialogue. Native dialogue choices and character-creation controls remain available.

Regression checks cover gaps between opening speech lines, appearance-menu stages, intro-save loading, ordinary saves, the stage-55 handoff and independent movement/combat flags. Plugin build and tests pass; the corrected opening still needs a fresh live run.

## Native gameplay cursor

Replaced the D3D rectangle arrow and aiming crosshair with the game's existing cursor TileImage and texture. The plugin submits that node through the native UI camera and shader accumulator during active isometric gameplay. The arrow turns red during Alt aiming or a queued attack, including a single shot at the ground.

Cursor position, colour and visibility changes are scoped to the additional render pass and restored immediately afterward. Menus, dialogue, character creation and disabled isometric mode keep native cursor ownership. No replacement cursor texture is distributed.

Validation: Release plugin build, camera, combat and context tests pass. Live Steam gameplay captures verify the native arrow, red colouring during a queued ground shot, and restoration to amber after the shot. Full opening/dialogue playthrough remains pending.

## Character-creation handoff recovery

The startup guard no longer obtains VCG01 state from compiled editor-ID expressions. The generic numeric-expression helper maps a failed query to zero, which can keep a fresh game blocked indefinitely. It now resolves base-game quest form 00104C1C through the native form table and reads TESQuest currentStage/flags directly. The stage-55 handoff still requires released movement, finished furniture animation and no active dialogue/menu.

Runtime status now reports opening_stage, opening_sequence, startup_waiting and native_control_flags. Regression checks cover both fresh games and loaded intro saves at the handoff, including the remaining native fight/Pip-Boy locks. Release build and combat checks pass; a live character-creation rerun is still required.

## Seated visibility, zoom and creature Search correction

Native mouse-look ownership now follows the camera through seated/SayTo gameplay,
while dialogue/menu input is retained. Scroll zoom is restored. The experimental
camera-transform and material-opacity overrides were removed, as was blanket
hiding of the first-person skeleton.

The couch reproduction is independent of dialogue. Read-only inspection of the
running game confirmed both the requested third-person flag and visible nodes,
and traced native rendered-body selection to 0x951A10. Eight validated callers
include direct calls that bypass ToggleFirstPerson (0x950110). These callers now
pass through a separate body-selection guard while the mod owns the camera.
Native body/animation bookkeeping is retained; manual node-unhiding is removed.
The main loop reconciles a mismatched rendered-body state through that same
native routine. Intro and Pip-Boy camera ownership remain exceptions.

Search and hover incorrectly applied the taken-item bit to actor references.
The shared eligibility function now reserves that bit for non-actor references,
and still rejects deleted or disabled actors. The context menu uses the same
eligibility as hover and activation instead of offering an unusable Search row.

Validation is limited to local builds and automated tests: creature Search,
reference flags, native body-switch ownership, intro/Pip-Boy passthrough, combat/startup
guards and wheel/menu input. The reported gecko and
couch scenes have not been replayed with this build. Desktop/game automation
requires the user's permission before any further live test.
