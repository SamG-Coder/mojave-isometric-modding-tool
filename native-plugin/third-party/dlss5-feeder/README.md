# DLSS5-Feeder attribution

Host and supporting headers originate from
https://github.com/jlrouzies-fr/DLSS5-Feeder at
`3f624855276c4bde55145c712782477639b30e85` (0.15.1).
They retain the accompanying MIT licence and original comments.

SamGCoder modifications add protocol 10's history-mask resource and pass it to
NGX; see native-host/README.md. The src/feed_ipc.h forwarding header keeps both
the x86 client and x64 host on the same protocol definition.

No Lumenite shader source is included. NVIDIA SDK files are external build
dependencies and retain NVIDIA's licence.
