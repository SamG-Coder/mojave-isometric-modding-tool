// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#pragma once
#include <d3d9.h>
namespace overlay {
// Keep small HUD clears independent: the D3D9On12/NVIDIA path can fail fast
// when a destination ring is submitted as one large rectangle array.
inline HRESULT clearRects(IDirect3DDevice9* device, DWORD count, const D3DRECT* rects, D3DCOLOR colour) {
    for (DWORD i = 0; i < count; ++i) {
        if (rects[i].x2 <= rects[i].x1 || rects[i].y2 <= rects[i].y1) continue;
        HRESULT hr = device->Clear(1, rects + i, D3DCLEAR_TARGET, colour, 1, 0);
        if (FAILED(hr)) return hr;
    }
    return D3D_OK;
}
}
