// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#pragma once
#include "../third-party/dlss5-feeder/feed_ipc.h"
#include <atomic>
#include <thread>
#include <vector>
#include <stdexcept>
#include <d3dcompiler.h>
#include "bridge_motion.hpp"
namespace renderer_bridge {
inline void require(HRESULT hr,const char* operation){if(FAILED(hr))throw std::runtime_error(std::string(operation)+" HRESULT="+std::to_string(static_cast<unsigned>(hr)));}
struct Handle {
 HANDLE value{};
 ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
 Handle()=default;Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
// IPC layout is pinned to DLSS5-Feeder 0.15.1, MIT; see third-party licence.
static_assert(sizeof(FeedHello)==24&&sizeof(FeedFrameMsg)==20);
class Channel {
 Handle pipe,process;std::thread worker;std::atomic<bool> stop{};
 bool transfer(void* data,DWORD size,bool write,DWORD timeout=15000){
  char* p=static_cast<char*>(data);auto deadline=GetTickCount64()+timeout;
  while(size&&!stop){
   Handle event;event.value=CreateEventW(nullptr,TRUE,FALSE,nullptr);OVERLAPPED ov{};ov.hEvent=event.value;DWORD count{};
   BOOL ok=write?WriteFile(pipe.value,p,size,&count,&ov):ReadFile(pipe.value,p,size,&count,&ov);
   if(!ok&&GetLastError()!=ERROR_IO_PENDING)return false;
   if(!ok){while(!stop&&GetTickCount64()<deadline&&WaitForSingleObject(event.value,10)==WAIT_TIMEOUT){}
    if(stop||GetTickCount64()>=deadline){CancelIoEx(pipe.value,&ov);GetOverlappedResult(pipe.value,&ov,&count,TRUE);return false;}
    if(!GetOverlappedResult(pipe.value,&ov,&count,FALSE))return false;
   }
   if(!count)return false;p+=count;size-=count;
  }return size==0;
 }
public:
 ComPtr<ID3D12Device> gpu;
 ComPtr<ID3D12Resource> textures[FEED_SLOTS];ComPtr<ID3D12Fence> input,output;
 std::atomic<int> state{}; // 0 connecting, 1 ready, -1 failed
 std::atomic<UINT64> pending{},sent{};
 std::atomic<UINT> resetHistory{1};
 UINT width{},height{};bool neural{};
 ~Channel(){stop=true;if(worker.joinable())worker.join();if(pipe.value&&pipe.value!=INVALID_HANDLE_VALUE){CloseHandle(pipe.value);pipe.value=nullptr;}
  if(process.value&&WaitForSingleObject(process.value,1000)==WAIT_TIMEOUT){TerminateProcess(process.value,1);WaitForSingleObject(process.value,1000);}}
 void start(ID3D12Device* device,UINT w,UINT h,bool nr,const std::filesystem::path& path){
  gpu=device;width=w;height=h;neural=nr;
  worker=std::thread([this,path]{try{
   auto exe=path/L"candidate/host64/dlss5-feed-host64.exe";auto cwd=exe.parent_path();
   std::wstring command=L"\""+exe.wstring()+L"\" "+std::to_wstring(GetCurrentProcessId())+L" --behind";
   STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;PROCESS_INFORMATION pi{};
   if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,cwd.c_str(),&si,&pi))throw std::runtime_error("Could not launch DLSS helper");
   process.value=pi.hProcess;CloseHandle(pi.hThread);
   char name[128];sprintf_s(name,FEED_PIPE_FMT,GetCurrentProcessId());auto until=GetTickCount64()+30000;
   while(!stop&&GetTickCount64()<until){pipe.value=CreateFileA(name,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);if(pipe.value!=INVALID_HANDLE_VALUE)break;Sleep(20);}
   if(stop||pipe.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Helper connection timed out");
   HANDLE remote{};if(!DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),process.value,&remote,PROCESS_DUP_HANDLE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,0))throw std::runtime_error("Duplicate process handle failed");
   FeedHello hello{FEED_IPC_MAGIC,FEED_IPC_VERSION,GetCurrentProcessId(),FEED_CLIENT_D3D11,static_cast<uint64_t>(reinterpret_cast<ULONG_PTR>(remote))};FeedHelloAck helloAck{};
   if(!transfer(&hello,sizeof(hello),true)||!transfer(&helloAck,sizeof(helloAck),false)||helloAck.magic!=FEED_IPC_MAGIC||helloAck.version!=FEED_IPC_VERSION)throw std::runtime_error("Helper protocol mismatch");
   FeedBuild build{};build.width=width;build.height=height;build.color_fmt=build.output_fmt=DXGI_FORMAT_R8G8B8A8_UNORM;build.flags_override=-1;build.transport=neural?0:1;build.mv_scale_x=build.mv_scale_y=1;build.client_flags=FEED_BUILD_ASYNC_HOME;
   // This is the protocol's client-created-texture route; the client device is D3D12.
   DXGI_FORMAT formats[]{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R16G16_FLOAT};
   Handle shared[FEED_SLOTS];
   for(int i=0;i<FEED_SLOTS;i++){
    D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=width;d.Height=height;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=formats[i];d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS|(i==FEED_OUTPUT?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    require(gpu->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_SHARED,&d,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&textures[i])),"Shared texture");
    require(gpu->CreateSharedHandle(textures[i].Get(),nullptr,GENERIC_ALL,nullptr,&shared[i].value),"Shared texture handle");build.tex[i]=reinterpret_cast<ULONG_PTR>(shared[i].value);
   }
   char tag='B';FeedBuildAck ack{};
   if(!transfer(&tag,1,true)||!transfer(&build,sizeof(build),true)||!transfer(&ack,sizeof(ack),false)||!ack.ok)throw std::runtime_error("Helper rejected texture set");
   Handle handles[2];handles[0].value=reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(ack.fence_in));handles[1].value=reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(ack.fence_out));
   require(gpu->OpenSharedHandle(handles[0].value,IID_PPV_ARGS(&input)),"Input fence");require(gpu->OpenSharedHandle(handles[1].value,IID_PPV_ARGS(&output)),"Output fence");
   record(neural?"Neural channel ready":"Transport channel ready");state=1;
   while(!stop){auto n=pending.load();if(n>sent){FeedFrameMsg frame{n,resetHistory.load(),0,0};tag='F';if(!transfer(&tag,1,true,2000)||!transfer(&frame,sizeof(frame),true,2000))throw std::runtime_error("Helper frame connection lost");sent=n;}else Sleep(1);}
  }catch(const std::exception& e){record(e.what());state=-1;}});
 }
};
inline void transition(ID3D12GraphicsCommandList* list,ID3D12Resource* resource,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){
 D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,from,to};list->ResourceBarrier(1,&b);
}
// Colour conversion is performed by sampling, preserving RGBA/BGRA semantics.
// Optical motion is estimated from adjacent native colour frames, not fabricated
// from the mouse or player position. Depth is sampled from the real D24S8 buffer.
inline constexpr char shader[]=R"(
Texture2D<float4> image:register(t0);
Texture2D<float> zbuffer:register(t1);
Texture2D<float4> history:register(t2);
cbuffer Params:register(b0){uint width;uint height;uint haveHistory;uint guides;float4 previousX;float4 previousY;};
float4 vs(uint id:SV_VertexID):SV_Position {return float4(id==2?3:-1,id==1?3:-1,0,1);}
float4 copy(float4 p:SV_Position):SV_Target {return float4(image.Load(int3(p.xy,0)).rgb,1);}
struct Guides {float4 color:SV_Target0;float depth:SV_Target1;float2 motion:SV_Target2;};
Guides feed(float4 p:SV_Position){
 int2 pixel=int2(p.xy);Guides o;o.color=float4(image.Load(int3(pixel,0)).rgb,1);o.depth=zbuffer.Load(int3(pixel,0));o.motion=0;
 if(haveHistory){float best=1e20;int2 winner=0;float4 point=float4(p.xy,o.depth,1);float2 prior=guides?float2(dot(previousX,point),dot(previousY,point)):p.xy;int2 base=int2(round(prior-p.xy));
  [unroll]for(int y=-2;y<=2;y+=2)[unroll]for(int x=-2;x<=2;x+=2){
   int2 q=clamp(pixel+base+int2(x,y),int2(0,0),int2(width-1,height-1));float3 delta=o.color.rgb-history.Load(int3(q,0)).rgb;
   float error=dot(delta,delta)+0.0002*(x*x+y*y);if(error<best){best=error;winner=int2(x,y);}
  }o.motion=float2(base+winner);
 }return o;
}
)";
class Pipeline {
public:
 ComPtr<IDirect3DDevice9On12> interop;ComPtr<ID3D12Device> gpu;ComPtr<ID3D12CommandQueue> queue;
 ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> local;
 ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> copyPSO,feedPSO;
 ComPtr<ID3D12DescriptorHeap> srv,rtv;ComPtr<ID3D12Resource> depthCopy,history,display;
 ComPtr<ID3D12Resource> heldBack,heldDepth;std::unique_ptr<Channel> channel;
 UINT width{},height{},srvStep{},rtvStep{};DXGI_FORMAT backFormat=DXGI_FORMAT_B8G8R8A8_UNORM;UINT64 serial{},frame{},submittedAt{},displayedFrame{};bool composited{};bool previous{},failed{},cachedReady{};
 engine::CameraSample camera,historyCamera;
 void init(IDirect3DDevice9* device,UINT w,UINT h){
  ComPtr<IDirect3DSurface9> initialBack;D3DSURFACE_DESC initialDesc{};require(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&initialBack),"Initial backbuffer");require(initialBack->GetDesc(&initialDesc),"Initial format");backFormat=initialDesc.Format==D3DFMT_X8R8G8B8?DXGI_FORMAT_B8G8R8X8_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM;
  width=w;height=h;require(device->QueryInterface(IID_PPV_ARGS(&interop)),"Game is not D3D9On12");require(interop->GetD3D12Device(IID_PPV_ARGS(&gpu)),"D3D12 device");
  D3D12_COMMAND_QUEUE_DESC q{};require(gpu->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"Queue");require(gpu->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"Allocator");require(gpu->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"List");require(list->Close(),"List close");require(gpu->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&local)),"Local fence");
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=6;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;require(gpu->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srv)),"SRV heap");hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;require(gpu->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"RTV heap");srvStep=gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);rtvStep=gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=3;D3D12_ROOT_PARAMETER params[2]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[0].DescriptorTable={1,&range};params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[1].Constants={0,0,12};D3D12_ROOT_SIGNATURE_DESC rs{};rs.NumParameters=2;rs.pParameters=params;
  ComPtr<ID3DBlob> signature,error;require(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&error),"Root signature");require(gpu->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)),"Root signature create");
  ComPtr<ID3DBlob> vs,ps,feed;require(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&error),"Vertex shader");require(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"copy","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ps,&error),"Copy shader");require(D3DCompile(shader,sizeof(shader),nullptr,nullptr,nullptr,"feed","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&feed,&error),"Guide shader");
  D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=root.Get();p.VS={vs->GetBufferPointer(),vs->GetBufferSize()};p.PS={ps->GetBufferPointer(),ps->GetBufferSize()};p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.RasterizerState.DepthClipEnable=TRUE;p.BlendState.RenderTarget[0].RenderTargetWriteMask=15;p.SampleMask=UINT_MAX;p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=1;p.RTVFormats[0]=backFormat;p.SampleDesc.Count=1;
  p.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;p.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};p.DepthStencilState.BackFace=p.DepthStencilState.FrontFace;
  for(auto& rt:p.BlendState.RenderTarget){rt.SrcBlend=rt.SrcBlendAlpha=D3D12_BLEND_ONE;rt.DestBlend=rt.DestBlendAlpha=D3D12_BLEND_ZERO;rt.BlendOp=rt.BlendOpAlpha=D3D12_BLEND_OP_ADD;rt.LogicOp=D3D12_LOGIC_OP_NOOP;}
  require(gpu->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&copyPSO)),"Copy PSO");p.PS={feed->GetBufferPointer(),feed->GetBufferSize()};p.NumRenderTargets=3;p.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;p.RTVFormats[1]=DXGI_FORMAT_R32_FLOAT;p.RTVFormats[2]=DXGI_FORMAT_R16G16_FLOAT;p.BlendState.IndependentBlendEnable=TRUE;for(auto& rt:p.BlendState.RenderTarget)rt.RenderTargetWriteMask=15;require(gpu->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&feedPSO)),"Feed PSO");
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R24G8_TYPELESS;require(gpu->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&depthCopy)),"Depth sampling texture");d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;require(gpu->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&history)),"History texture");require(gpu->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&display)),"Display texture");
  channel=std::make_unique<Channel>();channel->start(gpu.Get(),w,h,mode==2,directory);
 }
 D3D12_CPU_DESCRIPTOR_HANDLE cpu(bool target,UINT index){auto h=target?rtv->GetCPUDescriptorHandleForHeapStart():srv->GetCPUDescriptorHandleForHeapStart();h.ptr+=index*(target?rtvStep:srvStep);return h;}
 void view(ID3D12Resource* resource,UINT index,DXGI_FORMAT format){D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=format;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;gpu->CreateShaderResourceView(resource,&d,cpu(false,index));}
 bool idle(){return !local||local->GetCompletedValue()>=serial;}
 void cached(IDirect3DDevice9* device){
  if(!cachedReady||!idle())return;
  ComPtr<IDirect3DSurface9> back;require(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back),"Cached backbuffer");require(interop->UnwrapUnderlyingResource(back.Get(),queue.Get(),IID_PPV_ARGS(&heldBack)),"Cached unwrap");
  bool submitted=false;
  try{
   require(allocator->Reset(),"Cached allocator");require(list->Reset(allocator.Get(),copyPSO.Get()),"Cached list");
   transition(list.Get(),heldBack.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_RENDER_TARGET);transition(list.Get(),display.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   view(display.Get(),3,DXGI_FORMAT_R8G8B8A8_UNORM);gpu->CreateRenderTargetView(heldBack.Get(),nullptr,cpu(true,3));
   ID3D12DescriptorHeap* heaps[]{srv.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootSignature(root.Get());auto source=srv->GetGPUDescriptorHandleForHeapStart();source.ptr+=3*srvStep;list->SetGraphicsRootDescriptorTable(0,source);UINT constants[]{width,height,0,0};list->SetGraphicsRoot32BitConstants(1,4,constants,0);
   D3D12_VIEWPORT vp{0,0,float(width),float(height),0,1};D3D12_RECT rect{0,0,LONG(mode==1?width/2:width),LONG(height)};list->RSSetViewports(1,&vp);list->RSSetScissorRects(1,&rect);auto target=cpu(true,3);list->OMSetRenderTargets(1,&target,FALSE,nullptr);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
   transition(list.Get(),display.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);transition(list.Get(),heldBack.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COMMON);require(list->Close(),"Cached close");ID3D12CommandList* lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);submitted=true;require(queue->Signal(local.Get(),++serial),"Cached signal");
  }catch(...){if(submitted)queue->Signal(local.Get(),++serial);UINT64 v=serial;ID3D12Fence* f[]{local.Get()};interop->ReturnUnderlyingResource(back.Get(),submitted?1:0,&v,f);throw;}
  UINT64 v=serial;ID3D12Fence* f[]{local.Get()};require(interop->ReturnUnderlyingResource(back.Get(),1,&v,f),"Cached return");composited=true;
 }
 void draw(IDirect3DDevice9* device){
  composited=false;if(failed)return;
  if(channel->state<0){failed=true;return;}if(channel->state!=1||!idle())return;
  if(frame&&channel->output->GetCompletedValue()<frame){if(GetTickCount64()-submittedAt>(frame<4?15000:2000)){record("Helper exceeded its startup/frame deadline; native rendering retained");failed=true;}else cached(device);return;}
  if(channel->output->GetCompletedValue()==UINT64_MAX){failed=true;record("Fence invalid; device removal HRESULT="+std::to_string(static_cast<unsigned>(gpu->GetDeviceRemovedReason())));return;}
  ComPtr<IDirect3DSurface9> back,depth;require(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back),"Backbuffer");require(device->GetDepthStencilSurface(&depth),"Depth buffer");D3DSURFACE_DESC dd{},bd{};depth->GetDesc(&dd);back->GetDesc(&bd);
  if(dd.Width<width||dd.Height<height||dd.MultiSampleType!=D3DMULTISAMPLE_NONE||dd.Format!=D3DFMT_D24S8||bd.MultiSampleType!=D3DMULTISAMPLE_NONE||(bd.Format!=D3DFMT_A8R8G8B8&&bd.Format!=D3DFMT_X8R8G8B8)){record("Unsupported buffers: depth="+std::to_string(dd.Width)+"x"+std::to_string(dd.Height)+" fmt="+std::to_string(dd.Format)+" samples="+std::to_string(dd.MultiSampleType)+" colour="+std::to_string(bd.Width)+"x"+std::to_string(bd.Height)+" fmt="+std::to_string(bd.Format)+" samples="+std::to_string(bd.MultiSampleType));failed=true;return;}
  if(depthCopy->GetDesc().Width!=dd.Width||depthCopy->GetDesc().Height!=dd.Height){
   auto d=depthCopy->GetDesc();d.Width=dd.Width;d.Height=dd.Height;D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;
   ComPtr<ID3D12Resource> resized;require(gpu->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&resized)),"Pooled depth allocation");depthCopy=resized;
  }
  require(interop->UnwrapUnderlyingResource(back.Get(),queue.Get(),IID_PPV_ARGS(&heldBack)),"Unwrap colour");
  auto depthHR=interop->UnwrapUnderlyingResource(depth.Get(),queue.Get(),IID_PPV_ARGS(&heldDepth));if(FAILED(depthHR)){interop->ReturnUnderlyingResource(back.Get(),0,nullptr,nullptr);require(depthHR,"Unwrap depth");}
  // After both unwraps all exits return ownership, even if recording fails.
  bool submitted=false;
  try{
   require(allocator->Reset(),"Allocator reset");require(list->Reset(allocator.Get(),nullptr),"List reset");
   transition(list.Get(),heldDepth.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(list.Get(),depthCopy.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);
   // Copy both depth/stencil planes, preserving the resource's typeless layout.
   list->CopyResource(depthCopy.Get(),heldDepth.Get());transition(list.Get(),heldDepth.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON);transition(list.Get(),depthCopy.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   transition(list.Get(),heldBack.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);transition(list.Get(),history.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   view(heldBack.Get(),0,backFormat);view(depthCopy.Get(),1,DXGI_FORMAT_R24_UNORM_X8_TYPELESS);view(history.Get(),2,DXGI_FORMAT_R8G8B8A8_UNORM);view(display.Get(),3,DXGI_FORMAT_R8G8B8A8_UNORM);
   int slots[]{FEED_COLOR,FEED_DEPTH,FEED_MV};for(UINT i=0;i<3;i++){auto t=channel->textures[slots[i]].Get();transition(list.Get(),t,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_RENDER_TARGET);gpu->CreateRenderTargetView(t,nullptr,cpu(true,i));}gpu->CreateRenderTargetView(heldBack.Get(),nullptr,cpu(true,3));
   ID3D12DescriptorHeap* heaps[]{srv.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootSignature(root.Get());list->SetGraphicsRootDescriptorTable(0,srv->GetGPUDescriptorHandleForHeapStart());auto constants=motionConstants(width,height,previous,camera,historyCamera);list->SetGraphicsRoot32BitConstants(1,12,&constants,0);
   D3D12_VIEWPORT vp{0,0,float(width),float(height),0,1};D3D12_RECT sc{0,0,LONG(width),LONG(height)};list->RSSetViewports(1,&vp);list->RSSetScissorRects(1,&sc);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);auto targets=cpu(true,0);list->OMSetRenderTargets(3,&targets,TRUE,nullptr);list->SetPipelineState(feedPSO.Get());list->DrawInstanced(3,1,0,0);
   for(int slot:slots)transition(list.Get(),channel->textures[slot].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COMMON);
   transition(list.Get(),history.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);transition(list.Get(),channel->textures[FEED_COLOR].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE);list->CopyResource(history.Get(),channel->textures[FEED_COLOR].Get());transition(list.Get(),channel->textures[FEED_COLOR].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON);transition(list.Get(),history.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COMMON);
   if(frame&&previous){transition(list.Get(),heldBack.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);transition(list.Get(),channel->textures[FEED_OUTPUT].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(list.Get(),display.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);list->CopyResource(display.Get(),channel->textures[FEED_OUTPUT].Get());transition(list.Get(),channel->textures[FEED_OUTPUT].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON);transition(list.Get(),display.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);cachedReady=true;displayedFrame=frame;composited=true;auto output=srv->GetGPUDescriptorHandleForHeapStart();output.ptr+=3*srvStep;list->SetGraphicsRootDescriptorTable(0,output);auto target=cpu(true,3);list->OMSetRenderTargets(1,&target,FALSE,nullptr);list->SetPipelineState(copyPSO.Get());if(mode==1){D3D12_RECT half{0,0,LONG(width/2),LONG(height)};list->RSSetScissorRects(1,&half);}list->DrawInstanced(3,1,0,0);transition(list.Get(),display.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);transition(list.Get(),heldBack.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COMMON);}
   else transition(list.Get(),heldBack.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
   transition(list.Get(),depthCopy.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);require(list->Close(),"Submit close");ID3D12CommandList* lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);submitted=true;require(queue->Signal(local.Get(),++serial),"Local signal");require(queue->Signal(channel->input.Get(),++frame),"Host signal");channel->resetHistory=previous?0u:1u;channel->pending=frame;submittedAt=GetTickCount64();previous=true;historyCamera=camera;
   if(frame==1||frame%300==0)record("Submitted GPU frame "+std::to_string(frame)+" at "+std::to_string(width)+"x"+std::to_string(height));
  }catch(...){if(submitted)queue->Signal(local.Get(),++serial);UINT64 v=serial;ID3D12Fence* f[]{local.Get()};interop->ReturnUnderlyingResource(back.Get(),submitted?1:0,&v,f);interop->ReturnUnderlyingResource(depth.Get(),submitted?1:0,&v,f);throw;}
  UINT64 v=serial;ID3D12Fence* f[]{local.Get()};require(interop->ReturnUnderlyingResource(back.Get(),1,&v,f),"Return colour");require(interop->ReturnUnderlyingResource(depth.Get(),1,&v,f),"Return depth");
 }
};
inline std::unique_ptr<Pipeline> pipeline;
inline bool shutdown(){
 if(pipeline&&!pipeline->idle()){
  Handle event;event.value=CreateEventW(nullptr,FALSE,FALSE,nullptr);
  if(!event.value||FAILED(pipeline->local->SetEventOnCompletion(pipeline->serial,event.value))||WaitForSingleObject(event.value,2000)!=WAIT_OBJECT_0){record("GPU not idle; deferring device reset");return false;}
 }
 pipeline.reset();return true;
}
using Reset=HRESULT(WINAPI*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
inline Reset originalReset{};inline IDirect3DDevice9* resetOwner{};
inline HRESULT WINAPI resetDevice(IDirect3DDevice9* device,D3DPRESENT_PARAMETERS* parameters){
 if(device==resetOwner&&!shutdown())return D3DERR_DEVICELOST;
 return originalReset(device,parameters);
}
inline void installReset(IDirect3DDevice9* device){
 if(resetOwner==device)return;auto table=*reinterpret_cast<void***>(device);
 if(table[16]!=reinterpret_cast<void*>(&resetDevice)){
  DWORD old{};if(!VirtualProtect(table+16,sizeof(void*),PAGE_READWRITE,&old))throw std::runtime_error("Cannot install device reset guard");
  originalReset=reinterpret_cast<Reset>(table[16]);table[16]=reinterpret_cast<void*>(&resetDevice);DWORD ignored{};VirtualProtect(table+16,sizeof(void*),old,&ignored);
 }resetOwner=device;
}
inline void suspend(){if(pipeline){pipeline->previous=false;pipeline->cachedReady=false;pipeline->composited=false;}}
inline void frame(IDirect3DDevice9* device,const engine::CameraSample& camera={}){
 if(!mode)return;
 try{
  ComPtr<IDirect3DSurface9> back;D3DSURFACE_DESC d{};if(FAILED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back))||FAILED(back->GetDesc(&d)))return;
  if(pipeline&&(pipeline->width!=d.Width||pipeline->height!=d.Height)){if(!pipeline->idle())return;pipeline.reset();}
  if(!pipeline){installReset(device);pipeline=std::make_unique<Pipeline>();pipeline->init(device,d.Width,d.Height);}
  pipeline->camera=camera;pipeline->draw(device);
 }catch(const std::exception& e){record(e.what());if(pipeline)pipeline->failed=true;}
}
}
