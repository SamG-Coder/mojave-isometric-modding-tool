// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include <d3d9.h>
#include <vector>
#include <cmath>
namespace cutaway {
// A local screen-space stencil aperture preserves the rest of each occluder.
// Never touches scene-node visibility flags or replaces the game's materials.
class Mask {
    struct Vertex {float x,y,z,rhw;};
    IDirect3DDevice9* device{};IDirect3DStateBlock9* state{};
    void draw(const std::vector<Vertex>& vertices,DWORD ref){
        device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW);device->SetTexture(0,nullptr);
        device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE,0);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE,TRUE);device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE,FALSE);
        device->SetRenderState(D3DRS_STENCILMASK,0x80);device->SetRenderState(D3DRS_STENCILWRITEMASK,0x80);
        device->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_ALWAYS);device->SetRenderState(D3DRS_STENCILREF,ref);
        device->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_REPLACE);
        device->SetRenderState(D3DRS_STENCILFAIL,D3DSTENCILOP_KEEP);device->SetRenderState(D3DRS_STENCILZFAIL,D3DSTENCILOP_KEEP);
        device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(vertices.size()/3),vertices.data(),sizeof(Vertex));
    }
    static void quad(std::vector<Vertex>& out,float x,float y,float w,float h){
        Vertex a{x,y,0,1},b{x+w,y,0,1},c{x,y+h,0,1},d{x+w,y+h,0,1};out.insert(out.end(),{a,b,c,b,d,c});
    }
    std::vector<Vertex> bounds;
public:
    bool begin(IDirect3DDevice9* d,float x,float y,float radius){
        device=d;DWORD stencil{};device->GetRenderState(D3DRS_STENCILENABLE,&stencil);if(stencil)return false;
        IDirect3DSurface9* depth{};if(FAILED(device->GetDepthStencilSurface(&depth)))return false;
        D3DSURFACE_DESC desc{};depth->GetDesc(&desc);depth->Release();
        if(desc.Format!=D3DFMT_D24S8&&desc.Format!=D3DFMT_D24FS8)return false;
        if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&state)))return false;state->Capture();
        bounds.clear();quad(bounds,x-radius-3,y-radius-3,2*radius+6,2*radius+6);
        draw(bounds,0);
        std::vector<Vertex> aperture;
        // Ordered stippling at the rim gives a gradual cutaway without changing
        // opaque materials into incorrectly sorted transparent objects.
        for(int py=-int(radius);py<=int(radius);py+=3)for(int px=-int(radius);px<=int(radius);px+=3){
            float t=(radius-std::hypot(float(px),float(py)))/18.f;
            int pattern=((px+int(radius))/3+2*((py+int(radius))/3))&3;
            if(t>float(pattern)/4)quad(aperture,x+px,y+py,3,3);
        }
        if(!aperture.empty())draw(aperture,0x80);state->Apply();
        device->SetRenderState(D3DRS_STENCILENABLE,TRUE);device->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE,FALSE);
        device->SetRenderState(D3DRS_STENCILFUNC,D3DCMP_NOTEQUAL);device->SetRenderState(D3DRS_STENCILREF,0x80);
        device->SetRenderState(D3DRS_STENCILMASK,0x80);device->SetRenderState(D3DRS_STENCILWRITEMASK,0);
        device->SetRenderState(D3DRS_STENCILPASS,D3DSTENCILOP_KEEP);device->SetRenderState(D3DRS_STENCILFAIL,D3DSTENCILOP_KEEP);device->SetRenderState(D3DRS_STENCILZFAIL,D3DSTENCILOP_KEEP);
        return true;
    }
    void end(){if(!state)return;draw(bounds,0);state->Apply();state->Release();state=nullptr;}
};
}
