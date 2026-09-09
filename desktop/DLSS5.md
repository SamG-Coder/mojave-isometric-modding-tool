# Experimental independent DLSS 5 bridge

## Enable

Open **Settings > Isometric**, set **Experimental RTX Remix** to **Off** and
**Experimental DLSS 5** to **On**, then select **Apply**. Quit the game completely
and restart through the configured Steam Play launcher. Load a save to start
processing the isometric world image. No Remix Setup or scene capture is needed.

This is a single On/Off option. Quality/Balanced/Performance presets, frame
generation, ray reconstruction and ray tracing are not implemented by this
bridge. Do not interpret On as enabling every feature sold under the DLSS name.
The current helper configuration uses native-resolution neural processing.

The selection is stored separately in `runtime/dlss5/bridge.ini`: mode 0 is Off,
mode 2 is neural processing. Mode 1 is a developer transport diagnostic, not a
quality preset. The launcher temporarily disables MSAA for compatible depth
sharing, backs up its previous value, and restores it on a later Off launch if
the value has not otherwise been changed.

## Implementation

The native x86 plugin redirects the game's Direct3DCreate9 lookup to Windows'
system D3D9On12 implementation. It shares colour, real D24S8 depth and estimated
motion with a separate x64 DLSS5-Feeder helper using D3D12 textures and fences.
The helper loads RenoDX and NVIDIA NGX. ReShade runs in the helper only.
No game-root d3d9.dll, dgVoodoo translator or RTX Remix runtime is required.

Processing happens before the native HUD. The last completed image is retained
between neural updates; click projection uses that image's camera. Motion uses
orthographic camera/depth reprojection with a small image-based correction.
These are estimated vectors, not native per-object animation vectors. Processing
is suspended for native menus, dialogue and Pip-Boy transitions. Device reset
releases the bridge resources before forwarding to the game.

## Validation and limitations

Recorded on RTX 5080, driver 616.64, on 2026-09-09:

- The pinned helper completed 300 synthetic neural evaluations.
- The client completed 120 transport and 120 neural roundtrips at 640x360,
  including X8R8G8B8 colour and a larger pooled depth surface.
- A live 2560x1440 session submitted over 5,000 neural frames. Native HUD remained
  separate. Neural image updates were approximately 15 fps in the tested scene;
  this is not a demonstrated performance improvement.
- Camera reprojection agrees with reference world projection at 720p and 1440p.
- A later click crash was traced through the destination marker's multi-rectangle
  Clear call into D3D9On12/NVIDIA. Markers now clear one rectangle per call. The
  regression checks 120 draws and exact output pixels at 640x360 and 2560x1440.
  The patched marker has not yet been confirmed by a live clicking test.
- Full menu, dialogue, save/reload and device-reset gameplay validation remains
  incomplete. Fast motion and moving actors can produce temporal artefacts.

Logs are in `runtime/dlss5/bridge.log` and the helper folder. A saved On setting
alone does not prove the helper is processing: look for a fresh channel-ready
message followed by GPU-frame submissions. Errors retain native rendering where
possible; this experimental integration is not guaranteed crash-free.

## Dependencies

`desktop/setup-dlss5.ps1` provisions pinned components into an isolated runtime
folder, validates hashes and NVIDIA signatures, and invokes the helper smoke
test unless skipped. Its complete provisioning workflow still needs validation.
`desktop/test-dlss5.ps1` runs the standalone helper test. Stop the game before
running these scripts.

Pinned components: DLSS5-Feeder 0.15.1 (protocol 9), ReShade 6.8.0 addon runtime,
classic RenoDX DLSS5 4.5, NVIDIA DLSS 310.9.1 and DLSSNR 310.8.0. The newer tested
RenoDX 4.55 archive failed the helper test and is not used.

Upstream sources:
- https://github.com/jlrouzies-fr/DLSS5-Feeder/releases/tag/v0.15.1
- https://reshade.me/
- https://github.com/RankFTW/rhi-repo/releases/tag/renodx-dlss5-4.5
- https://github.com/RankFTW/rhi-repo/releases/tag/dlssnr-310.8.0
- https://github.com/RankFTW/rhi-repo/releases/tag/dlss-310.9.1

Runtime binaries, profiles and logs are ignored by Git. Third-party components
retain their upstream licences and are not relicensed under this project's GPL.
The vendored IPC header retains its MIT licence in native-plugin/third-party.
