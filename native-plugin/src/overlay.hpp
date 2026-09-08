// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include <windows.h>
#include <d3d9.h>
#include <vector>
#include <string>
#include <algorithm>
namespace overlay {
struct Vertex {float x,y,z=0,rhw=1;DWORD colour;float u=0,v=0;};
class Painter {
    IDirect3DDevice9* device{};IDirect3DTexture9* font{};IDirect3DStateBlock9* state{};bool scene{},ownsScene{};
    std::vector<Vertex> geometry,glyphs;
    void quad(std::vector<Vertex>& out,float x,float y,float w,float h,DWORD c,float u=0,float v=0,float du=0,float dv=0){
        Vertex a{x,y,0,1,c,u,v},b{x+w,y,0,1,c,u+du,v},d{x,y+h,0,1,c,u,v+dv},e{x+w,y+h,0,1,c,u+du,v+dv};out.insert(out.end(),{a,b,d,b,e,d});
    }
    void makeFont(){
        if(font)return;
        constexpr int W=256,H=128;HDC dc=CreateCompatibleDC(nullptr);if(!dc)return;
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=W;info.bmiHeader.biHeight=-H;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        void* pixels{};HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!bitmap){DeleteDC(dc);return;}
        auto oldBitmap=SelectObject(dc,bitmap);auto type=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH,L"Consolas");auto oldFont=SelectObject(dc,type);
        memset(pixels,0,W*H*4);SetBkColor(dc,RGB(0,0,0));SetTextColor(dc,RGB(255,255,255));
        for(int i=32;i<127;i++){char c=char(i);TextOutA(dc,(i%16)*16,(i/16)*16,&c,1);}
        GdiFlush();
        if(SUCCEEDED(device->CreateTexture(W,H,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&font,nullptr))){D3DLOCKED_RECT lock{};if(SUCCEEDED(font->LockRect(0,&lock,nullptr,0))){for(int y=0;y<H;y++)for(int x=0;x<W;x++){DWORD src=static_cast<DWORD*>(pixels)[y*W+x];DWORD alpha=src&255;reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]=(alpha<<24)|0xffffff;}font->UnlockRect(0);}}
        SelectObject(dc,oldFont);SelectObject(dc,oldBitmap);DeleteObject(type);DeleteObject(bitmap);DeleteDC(dc);
    }
public:
    bool begin(IDirect3DDevice9* d){device=d;geometry.clear();glyphs.clear();if(FAILED(device->CreateStateBlock(D3DSBT_ALL,&state)))return false;state->Capture();HRESULT beginResult=device->BeginScene();ownsScene=SUCCEEDED(beginResult);if(FAILED(beginResult)&&beginResult!=D3DERR_INVALIDCALL){state->Release();state=nullptr;return false;}scene=true;makeFont();return true;}
    void rect(float x,float y,float w,float h,DWORD c){quad(geometry,x,y,w,h,c);}
    void line(float x,float y,float bx,float by,DWORD c){float dx=bx-x,dy=by-y;int n=int(std::max(std::abs(dx),std::abs(dy)));for(int i=0;i<=n;i++){float t=n?float(i)/n:0;rect(x+dx*t,y+dy*t,1.5f,1.5f,c);}}
    void text(float x,float y,const std::string& s,DWORD c){for(unsigned char ch:s){if(ch<32||ch>126)ch='?';quad(glyphs,x,y,16,16,c,float(ch%16)/16,float(ch/16)/8,1.f/16,1.f/8);x+=8;}}
    void finish(){
        if(!scene)return;device->SetVertexShader(nullptr);device->SetPixelShader(nullptr);device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);device->SetRenderState(D3DRS_LIGHTING,FALSE);device->SetRenderState(D3DRS_FOGENABLE,FALSE);device->SetRenderState(D3DRS_STENCILENABLE,FALSE);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
        device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);device->SetTexture(0,nullptr);
        if(!geometry.empty())device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(geometry.size()/3),geometry.data(),sizeof(Vertex));
        if(font&&!glyphs.empty()){device->SetTexture(0,font);device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);device->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_TEXTURE);device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);device->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_TEXTURE);device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,UINT(glyphs.size()/3),glyphs.data(),sizeof(Vertex));}
        if(ownsScene)device->EndScene();state->Apply();state->Release();state=nullptr;scene=false;
    }
};
}
