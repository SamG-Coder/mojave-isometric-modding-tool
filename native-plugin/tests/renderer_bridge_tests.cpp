// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#include "../src/renderer_bridge.hpp"
#include "../src/dlss_bridge_channel.hpp"
#include <cstdio>
#include <stdexcept>
#include <d3d12sdklayers.h>
void check(HRESULT hr,const char* what){if(FAILED(hr)){printf("FAIL %s: %08X\n",what,unsigned(hr));throw std::runtime_error(what);}}
int main(int argc,char** argv){
 using namespace renderer_bridge; const UINT W=argc>1?640:64,H=argc>1?360:64;
 try{
 for(auto resolution: {std::pair<UINT,UINT>{1280,720},{2560,1440}}){
  engine::CameraSample before{{0,0,1000},{0,0,-1},{0,1,0},{1,0,0},{-500,500,281.25f,-281.25f,1,2000,true,{}},0,0,float(resolution.first),float(resolution.second),true};
  auto now=before;now.position.x+=40;now.right={0.98480775f,0.17364818f,0};now.up={-0.17364818f,0.98480775f,0};
  auto c=motionConstants(resolution.first,resolution.second,true,now,before);if(!c.reproject)throw std::runtime_error("Camera motion rejected");
  for(float u:{.3f,.5f,.7f}){float px=now.width*u,py=now.height*.4f,z=.25f;engine::Vec origin,dir;now.ray(px,py,origin,dir);auto point=origin+dir*(now.frustum.nearPlane+z*(now.frustum.farPlane-now.frustum.nearPlane));float expectedX{},expectedY{};if(!before.project(point,expectedX,expectedY))throw std::runtime_error("Reference projection failed");float x=c.previousX[0]*px+c.previousX[1]*py+c.previousX[2]*z+c.previousX[3],y=c.previousY[0]*px+c.previousY[1]*py+c.previousY[2]*z+c.previousY[3];if(std::abs(x-expectedX)>.01f||std::abs(y-expectedY)>.01f)throw std::runtime_error("Motion reprojection differs from camera ray/project");}
 }
 printf("PASS: camera motion reprojection agrees with world-space projection at 720p and 1440p.\n");
 ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
 directory=std::filesystem::temp_directory_path();
 auto window=CreateWindowExW(0,L"STATIC",L"Mojave GPU bridge test",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 if(!window)return 1;
 ComPtr<IDirect3D9> api;api.Attach(create(D3D_SDK_VERSION));if(!api)return 1;
 D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.BackBufferWidth=W;pp.BackBufferHeight=H;pp.BackBufferFormat=argc>1?D3DFMT_X8R8G8B8:D3DFMT_A8R8G8B8;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;
 ComPtr<IDirect3DDevice9> device;check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,&pp,&device),"CreateDevice");
 ComPtr<IDirect3DDevice9On12> interop;check(device.As(&interop),"Query9On12");
 ComPtr<ID3D12Device> gpu;check(interop->GetD3D12Device(IID_PPV_ARGS(&gpu)),"GetD3D12Device");
 D3D12_COMMAND_QUEUE_DESC qd{};ComPtr<ID3D12CommandQueue> queue;check(gpu->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"Queue");
 ComPtr<IDirect3DSurface9> pooled;if(argc>1){check(device->CreateDepthStencilSurface(W,H+128,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,FALSE,&pooled,nullptr),"Pooled depth");check(device->SetDepthStencilSurface(pooled.Get()),"Set pooled depth");}
 ComPtr<IDirect3DSurface9> depth;check(device->GetDepthStencilSurface(&depth),"Depth9");ComPtr<ID3D12Resource> depth12;
 auto depthResult=interop->UnwrapUnderlyingResource(depth.Get(),queue.Get(),IID_PPV_ARGS(&depth12));printf("Depth unwrap: %08X\n",unsigned(depthResult));
 if(SUCCEEDED(depthResult)){auto dd=depth12->GetDesc();printf("Depth format %u flags %u\n",dd.Format,dd.Flags);check(interop->ReturnUnderlyingResource(depth.Get(),0,nullptr,nullptr),"Return depth");}
 ComPtr<IDirect3DSurface9> back;check(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back),"Backbuffer");
 check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff123456,1,0),"Clear9");
 ComPtr<ID3D12Resource> resource;check(interop->UnwrapUnderlyingResource(back.Get(),queue.Get(),IID_PPV_ARGS(&resource)),"Unwrap");
 auto desc=resource->GetDesc();printf("D3D9On12 resource %llu x %u format %u flags %u\n",desc.Width,desc.Height,desc.Format,desc.Flags);
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{};gpu->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
 D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rd.Width=bytes;rd.Height=1;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.SampleDesc.Count=1;rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 ComPtr<ID3D12Resource> readback;check(gpu->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)),"Readback");
 ComPtr<ID3D12CommandAllocator> allocator;check(gpu->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"Allocator");
 ComPtr<ID3D12GraphicsCommandList> list;check(gpu->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"List");
 D3D12_RESOURCE_BARRIER bar{};bar.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;bar.Transition={resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE};list->ResourceBarrier(1,&bar);
 D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=resource.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
 D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=readback.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
 std::swap(bar.Transition.StateBefore,bar.Transition.StateAfter);list->ResourceBarrier(1,&bar);check(list->Close(),"Close");ID3D12CommandList* lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);
 ComPtr<ID3D12Fence> fence;check(gpu->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Fence");check(queue->Signal(fence.Get(),1),"Signal");
 UINT64 value=1;ID3D12Fence* fences[]{fence.Get()};check(interop->ReturnUnderlyingResource(back.Get(),1,&value,fences),"Return");
 auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(fence->SetEventOnCompletion(1,event),"Event");if(WaitForSingleObject(event,5000)!=WAIT_OBJECT_0)throw std::runtime_error("GPU timeout");CloseHandle(event);
 void* data{};check(readback->Map(0,nullptr,&data),"Map");bool exact=true;for(UINT y=0;y<H;y++)for(UINT x=0;x<W;x++)if((reinterpret_cast<unsigned*>(static_cast<char*>(data)+y*footprint.Footprint.RowPitch)[x]&0xffffff)!=0x123456)exact=false;readback->Unmap(0,nullptr);
 if(!exact)throw std::runtime_error("Pixel mismatch");
 printf("PASS: all input pixels transferred exactly from D3D9 to D3D12; resource ownership returned.\n");
 if(argc>1){directory=argv[1];mode=argc>2?atoi(argv[2]):1;Pipeline bridge;bridge.init(device.Get(),W,H);auto end=GetTickCount64()+45000;
  while(!bridge.failed&&bridge.frame<120&&GetTickCount64()<end){check(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xff123456,0.5f,0),"Clear frame");bridge.draw(device.Get());Sleep(5);}
  ComPtr<ID3D12InfoQueue> info;if(SUCCEEDED(bridge.gpu.As(&info))){for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T size{};info->GetMessage(i,nullptr,&size);std::vector<char> storage(size);auto m=reinterpret_cast<D3D12_MESSAGE*>(storage.data());info->GetMessage(i,m,&size);if(m->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)printf("D3D12: %s\n",m->pDescription);}}
  if(bridge.failed||bridge.frame<120)throw std::runtime_error("Bridge roundtrip did not complete 120 frames");
  auto wait=CreateEventW(nullptr,FALSE,FALSE,nullptr);bridge.local->SetEventOnCompletion(bridge.serial,wait);if(WaitForSingleObject(wait,5000)!=WAIT_OBJECT_0)throw std::runtime_error("Bridge GPU timeout");bridge.channel->output->SetEventOnCompletion(bridge.frame,wait);if(WaitForSingleObject(wait,5000)!=WAIT_OBJECT_0)throw std::runtime_error("Host GPU timeout");CloseHandle(wait);
  for(int slot=0;slot<2;slot++){auto tex=bridge.channel->textures[slot].Get();auto desc2=tex->GetDesc();gpu->GetCopyableFootprints(&desc2,0,1,0,&footprint,nullptr,nullptr,&bytes);allocator->Reset();list->Reset(allocator.Get(),nullptr);transition(list.Get(),tex,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE);src.pResource=tex;dst.PlacedFootprint=footprint;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);transition(list.Get(),tex,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON);list->Close();queue->ExecuteCommandLists(1,lists);queue->Signal(fence.Get(),2+slot);auto ev=CreateEventW(nullptr,FALSE,FALSE,nullptr);fence->SetEventOnCompletion(2+slot,ev);WaitForSingleObject(ev,5000);CloseHandle(ev);readback->Map(0,nullptr,&data);auto px=static_cast<unsigned*>(data);printf("Shared slot %d: %08X %08X %08X\n",slot,px[0],px[31],px[63]);readback->Unmap(0,nullptr);}
  ComPtr<IDirect3DSurface9> pixels;check(device->CreateOffscreenPlainSurface(W,H,pp.BackBufferFormat,D3DPOOL_SYSTEMMEM,&pixels,nullptr),"Test staging");check(device->GetRenderTargetData(back.Get(),pixels.Get()),"Read returned frame");D3DLOCKED_RECT lock{};check(pixels->LockRect(&lock,nullptr,D3DLOCK_READONLY),"Returned pixels");unsigned first=*static_cast<unsigned*>(lock.pBits);bool same=true;for(UINT y=0;y<H;y++)for(UINT x=0;x<W;x++)if((reinterpret_cast<unsigned*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]&0xffffff)!=0x123456){if(same)printf("Mismatch at %u,%u: %08X\n",x,y,reinterpret_cast<unsigned*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]);same=false;}printf("Returned first pixel: %08X\n",first);for(UINT x=0;x<W;x+=8)printf("x%u pixel %08X\n",x,reinterpret_cast<unsigned*>(lock.pBits)[x]);pixels->UnlockRect();if(mode==1&&!same)throw std::runtime_error("Transport changed colours");printf("PASS: 120 GPU frames returned through 64-bit helper, mode %d\n",mode);
 }
 DestroyWindow(window);return 0;
 }catch(const std::exception& e){printf("FAIL: %s\n",e.what());return 1;}
}
