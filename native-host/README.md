# Native neural helper (x64)

This subproject builds `MojaveIsoNeuralHost.exe`, an MIT-licensed derivative of
DLSS5-Feeder 0.15.1 (upstream commit
`3f624855276c4bde55145c712782477639b30e85`). See the retained licence and source
under `native-plugin/third-party/dlss5-feeder`.

Mojave protocol 10 adds a full-resolution R8 history rejection texture to the
four original GPU textures. The helper validates it and passes it as
`pInBiasCurrentColorMask` during NGX evaluation. Both sides reject incompatible
protocol versions. This is not binary-compatible with the stock v9 helper.

Build separately from the x86 game plugin using Visual Studio and the official
[NVIDIA DLSS SDK](https://github.com/NVIDIA/DLSS). The SDK headers/libraries and
NVIDIA runtime binaries are not included or relicensed.

```powershell
cmake -S native-host -B build/host64 -A x64 -DNGX_SDK=C:/path/to/DLSS
cmake --build build/host64 --config Release
```

Validated SDK revision: `374959484e79a640feaba44c93ac8cfb0a03f5b5`.
`desktop/setup-dlss5.ps1` copies the locally built helper alongside the pinned
runtime dependencies. The native plugin explicitly starts this executable;
it does not load the package's ReShade addon into New Vegas.

Source changes from upstream: additional texture slot/protocol version, R8
format validation, mask evaluation argument and diagnostic logging. Upstream
compatibility paths remain in the source; only the native D3D12 client-created
texture path has been tested for this project.
