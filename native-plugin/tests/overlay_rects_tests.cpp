// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#include "../src/renderer_bridge.hpp"
#include "../src/overlay_rects.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
void check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("D3D operation failed"); }
int main() {
    using namespace renderer_bridge;
    try {
        directory = std::filesystem::temp_directory_path();
        auto window = CreateWindowExW(0,L"STATIC",L"Marker regression",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        ComPtr<IDirect3D9> api; api.Attach(create(D3D_SDK_VERSION));
        if (!window || !api) throw std::runtime_error("No test device");
        for (UINT width : {640u,2560u}) {
            UINT height = width * 9 / 16;
            D3DPRESENT_PARAMETERS pp{}; pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
            pp.hDeviceWindow=window; pp.BackBufferWidth=width; pp.BackBufferHeight=height; pp.BackBufferFormat=D3DFMT_X8R8G8B8;
            ComPtr<IDirect3DDevice9> device;
            check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,&pp,&device));
            check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff123456,1,0));
            std::array<D3DRECT,64> ring{};
            for (int i=0;i<64;++i) {
                float a=i*6.2831853f/64; LONG x=LONG(width/2+std::cos(a)*20),y=LONG(height/2+std::sin(a)*14);
                ring[i]={x-2,y-2,x+3,y+3};
            }
            for (int frame=0;frame<120;++frame) check(overlay::clearRects(device.Get(),DWORD(ring.size()),ring.data(),0xffffaa00));
            ComPtr<IDirect3DSurface9> back,pixels;
            check(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back));
            check(device->CreateOffscreenPlainSurface(width,height,pp.BackBufferFormat,D3DPOOL_SYSTEMMEM,&pixels,nullptr));
            check(device->GetRenderTargetData(back.Get(),pixels.Get()));
            D3DLOCKED_RECT lock{}; check(pixels->LockRect(&lock,nullptr,D3DLOCK_READONLY));
            bool exact=true;
            for (UINT y=0;y<height;++y) for (UINT x=0;x<width;++x) {
                DWORD expected=0x123456;
                for (const auto& r:ring) if (LONG(x)>=r.x1&&LONG(x)<r.x2&&LONG(y)>=r.y1&&LONG(y)<r.y2) {expected=0xffaa00;break;}
                if ((reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]&0xffffff)!=expected) exact=false;
            }
            pixels->UnlockRect();
            if (!exact) throw std::runtime_error("Marker pixels differ");
            printf("PASS: 120 destination-marker draws, exact pixels at %ux%u\n",width,height);
        }
        DestroyWindow(window); return 0;
    } catch (const std::exception& e) {printf("FAIL: %s\n",e.what());return 1;}
}
