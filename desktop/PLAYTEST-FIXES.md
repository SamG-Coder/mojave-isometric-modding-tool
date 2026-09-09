# Playtest fixes: pickup, dialogue and melee

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
