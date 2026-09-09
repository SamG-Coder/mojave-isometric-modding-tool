// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
// Included inside renderer_bridge. Original implementation; no Lumenite code.
inline constexpr char flowShader[]=R"(
Texture2D<float2> currentLevel:register(t0);
Texture2D<float2> priorLevel:register(t1);
Texture2D<float4> coarse:register(t2);
Texture2D<float4> priorFlow:register(t3);
Texture2D<float> depthImage:register(t4);
Texture2D<float2> priorFull:register(t5);
Texture2D<float4> colorImage:register(t6);
Texture2D<float> externalConfidence:register(t7);
RWTexture2D<float2> pyramidOut:register(u0);
RWTexture2D<float4> flowOut:register(u1);
RWTexture2D<float> maskOut:register(u2);
RWTexture2D<float2> motionOut:register(u3);
cbuffer Params:register(b0){uint width,height,hasHistory,hasCamera;float4 previousX,previousY,previousZ;uint level,passWidth,passHeight,hasCoarse;};
int2 bounded(int2 p,uint2 size){return clamp(p,int2(0,0),int2(size)-1);}
float lum(float3 c){return dot(c,float3(.2126,.7152,.0722));}
float2 cameraMotion(float2 p,float z){float4 v=float4(p,z,1);return hasCamera?float2(dot(previousX,v),dot(previousY,v))-p:0;}
float expectedDepth(float2 p,float z){return hasCamera?dot(previousZ,float4(p,z,1)):z;}
[numthreads(8,8,1)] void base(uint3 id:SV_DispatchThreadID){if(id.x>=width||id.y>=height)return;pyramidOut[id.xy]=float2(lum(colorImage.Load(int3(id.xy,0)).rgb),depthImage.Load(int3(id.xy,0)));}
[numthreads(8,8,1)] void reduce(uint3 id:SV_DispatchThreadID){
 if(id.x>=passWidth||id.y>=passHeight)return;uint w,h;currentLevel.GetDimensions(w,h);float light=0,z=1;
 [unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){float2 v=currentLevel.Load(int3(bounded(int2(id.xy)*2+int2(x,y),uint2(w,h)),0));light+=v.x;z=min(z,v.y);}
 pyramidOut[id.xy]=float2(light*.25,z);
}
// Zero-mean patch error separates structure from uniform brightness changes.
float score(int2 p,float2 displacement,out float variance){
 float a[9],b[9],ma=0,mb=0;int k=0;
 [unroll]for(int y=-1;y<=1;y++)[unroll]for(int x=-1;x<=1;x++){
  int2 q=bounded(p+int2(x,y),uint2(passWidth,passHeight));
  a[k]=currentLevel.Load(int3(q,0)).x;b[k]=priorLevel.Load(int3(bounded(int2(round(float2(q)+displacement)),uint2(passWidth,passHeight)),0)).x;ma+=a[k];mb+=b[k];k++;
 }ma/=9;mb/=9;float cost=0;variance=0;
 [unroll]for(int i=0;i<9;i++){float av=a[i]-ma,bv=b[i]-mb;cost+=(av-bv)*(av-bv);variance+=av*av;}
 return cost/9;
}
[numthreads(8,8,1)] void estimate(uint3 id:SV_DispatchThreadID){
 if(id.x>=passWidth||id.y>=passHeight)return;int2 p=id.xy;float scale=float(1u<<level);float2 full=(float2(p)+.5)*scale;float z=currentLevel.Load(int3(p,0)).y;
 float2 geometric=cameraMotion(full,z);float2 seed=geometric;
 if(hasCoarse){uint w,h;coarse.GetDimensions(w,h);float4 v=coarse.Load(int3(bounded(p/2,uint2(w,h)),0));if(v.z>.05)seed=v.xy;}
 float variance;float baseline=score(p,geometric/scale,variance);float best=baseline;float2 winner=geometric/scale;
 if(hasHistory){
  [loop]for(int search=0;search<2;search++)[loop]for(int y=-2;y<=2;y++)[loop]for(int x=-2;x<=2;x++){
   float2 candidate=(search==0?geometric:seed)/scale+float2(x,y);float unused;float cost=score(p,candidate,unused);
   if(cost<best){best=cost;winner=candidate;}
  }
 }
 // Only override the exact camera vector when a textured moving patch explains
 // the current structure materially better. Flat/flickering patches stay geometric.
 bool dynamic=hasHistory&&variance>.00008&&best<baseline*.70&&length(winner*scale-geometric)>.5;
 float2 result=dynamic?winner*scale:geometric;
 flowOut[id.xy]=float4(result,dynamic?saturate(1-best/max(variance,.0001)):0,z);
}
[numthreads(8,8,1)] void resolve(uint3 id:SV_DispatchThreadID){
 if(id.x>=width||id.y>=height)return;int2 p=id.xy;float2 full=float2(p)+.5;float2 current=currentLevel.Load(int3(p,0));float z=current.y;float2 geometric=cameraMotion(full,z),mv=geometric;float confidence=0;
 uint fw,fh;coarse.GetDimensions(fw,fh);float closest=1e20;
 // Depth-aware upsample prevents a foreground actor dragging its background.
 [unroll]for(int y=0;y<2;y++)[unroll]for(int x=0;x<2;x++){
  int2 q=bounded(int2(float2(p)*float2(fw,fh)/float2(width,height))+int2(x,y),uint2(fw,fh));float4 v=coarse.Load(int3(q,0));if(hasCoarse==2){v.xy*=float2(width,height);v.z=externalConfidence.Load(int3(q,0));int2 fullSample=bounded(int2((float2(q)+.5)*float2(width,height)/float2(fw,fh)),uint2(width,height));v.w=currentLevel.Load(int3(fullSample,0)).y;}float cost=abs(v.w-z)+.00001*length(float2(q)*4-full);
  if(cost<closest){closest=cost;if(v.z>.1&&abs(v.w-z)<.003){mv=v.xy;confidence=v.z;}else{mv=geometric;confidence=0;}}
 }
 float low=current.x,high=current.x;[unroll]for(int y=-1;y<=1;y++)[unroll]for(int x=-1;x<=1;x++){float l=currentLevel.Load(int3(bounded(p+int2(x,y),uint2(width,height)),0)).x;low=min(low,l);high=max(high,l);}
 if(high-low<.02){mv=geometric;confidence=0;}
 else if(confidence>.1&&hasHistory&&hasCoarse!=2){
  float variance;float best=score(p,mv,variance);float2 refined=mv;
  [unroll]for(int yy=-1;yy<=1;yy++)[unroll]for(int xx=-1;xx<=1;xx++){float unused;float cost=score(p,mv+float2(xx,yy),unused);if(cost<best){best=cost;refined=mv+float2(xx,yy);}}
  // Parabolic local refinement retains fractional object displacement.
  float unused;float l=score(p,refined-float2(1,0),unused),r=score(p,refined+float2(1,0),unused);
  float t=score(p,refined-float2(0,1),unused),b=score(p,refined+float2(0,1),unused);
  float2 denom=float2(l+r-2*best,t+b-2*best);
  mv=refined+clamp(float2(l-r,t-b)/max(2*denom,.000001),-.5,.5);
 }
 float2 previousPixel=full+mv;bool outside=any(previousPixel<.5)||previousPixel.x>=width||previousPixel.y>=height;
 int2 q=bounded(int2(previousPixel),uint2(width,height));float2 old=priorFull.Load(int3(q,0));
 float expected=expectedDepth(full,z);float depthError=abs(old.y-expected);

 bool lightMismatch=old.x<low-.08||old.x>high+.08;
 float4 previousVector=priorFlow.Load(int3(q,0));bool inconsistent=confidence>.1&&previousVector.z>.1&&length(mv-previousVector.xy)>max(12.,length(mv)*.75);
 float reject=(!hasHistory||outside||depthError>.003||inconsistent)?1:(lightMismatch?.75:0);
 if(confidence>.1&&reject>=1)mv=geometric;
 if(!hasHistory)mv=0;
 motionOut[p]=mv;maskOut[p]=reject;flowOut[p]=float4(mv,confidence,z);
}
)";

class MotionFlow {
 static constexpr UINT levels=6;
 ComPtr<ID3D12Device> gpu;ComPtr<ID3D12RootSignature> root;
 ComPtr<ID3D12PipelineState> basePSO,reducePSO,estimatePSO,resolvePSO;
 ComPtr<ID3D12DescriptorHeap> descriptors;
 ComPtr<ID3D12Resource> current[levels],old[levels],flow[levels],previousFlow,fullFlow;
 UINT width{},height{},step{},slot{};bool history{};
 struct Params {MotionConstants camera;UINT level,width,height,coarse;};
 static_assert(sizeof(Params)==80);
 ComPtr<ID3D12Resource> texture(UINT w,UINT h,DXGI_FORMAT format){
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=format;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  ComPtr<ID3D12Resource> result;require(gpu->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&result)),"Motion texture");return result;
 }
 D3D12_CPU_DESCRIPTOR_HANDLE cpu(UINT index){auto h=descriptors->GetCPUDescriptorHandleForHeapStart();h.ptr+=index*step;return h;}
 void srv(ID3D12Resource* resource,UINT index,DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN){
  D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=format==DXGI_FORMAT_UNKNOWN?(resource?resource->GetDesc().Format:DXGI_FORMAT_R32G32_FLOAT):format;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;gpu->CreateShaderResourceView(resource,&d,cpu(index));
 }
 void uav(ID3D12Resource* resource,UINT index,DXGI_FORMAT fallback){D3D12_UNORDERED_ACCESS_VIEW_DESC d{};d.Format=resource?resource->GetDesc().Format:fallback;d.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;gpu->CreateUnorderedAccessView(resource,nullptr,&d,cpu(index));}
 void dispatch(ID3D12GraphicsCommandList* list,ID3D12PipelineState* pso,Params params,ID3D12Resource* const* inputs,ID3D12Resource* const* outputs,DXGI_FORMAT depthFormat=DXGI_FORMAT_R32_FLOAT){
  UINT start=slot;slot+=12;
  for(UINT i=0;i<8;i++)srv(inputs[i],start+i,i==4?depthFormat:DXGI_FORMAT_UNKNOWN);
  DXGI_FORMAT formats[]{DXGI_FORMAT_R32G32_FLOAT,DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R8_UNORM,DXGI_FORMAT_R16G16_FLOAT};
  for(UINT i=0;i<4;i++)uav(outputs[i],start+8+i,formats[i]);
  ID3D12DescriptorHeap* heaps[]{descriptors.Get()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());list->SetPipelineState(pso);
  auto handle=descriptors->GetGPUDescriptorHandleForHeapStart();handle.ptr+=start*step;list->SetComputeRootDescriptorTable(0,handle);handle.ptr+=8*step;list->SetComputeRootDescriptorTable(1,handle);list->SetComputeRoot32BitConstants(2,20,&params,0);
  list->Dispatch((params.width+7)/8,(params.height+7)/8,1);
 }
 void copy(ID3D12GraphicsCommandList* list,ID3D12Resource* source,ID3D12Resource* target){transition(list,source,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(list,target,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);list->CopyResource(target,source);transition(list,source,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON);transition(list,target,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COMMON);}
public:
 void init(ID3D12Device* device,UINT w,UINT h){
  gpu=device;width=w;height=h;D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=160;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;require(gpu->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&descriptors)),"Motion descriptors");step=gpu->GetDescriptorHandleIncrementSize(hd.Type);
  D3D12_DESCRIPTOR_RANGE ranges[2]{};ranges[0]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,8,0,0,0};ranges[1]={D3D12_DESCRIPTOR_RANGE_TYPE_UAV,4,0,0,0};D3D12_ROOT_PARAMETER params[3]{};
  for(int i=0;i<2;i++){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[i].DescriptorTable={1,&ranges[i]};}params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[2].Constants={0,0,20};D3D12_ROOT_SIGNATURE_DESC desc{};desc.NumParameters=3;desc.pParameters=params;
  ComPtr<ID3DBlob> signature,error;require(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&error),"Motion root");require(gpu->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)),"Motion root create");
  const char* entries[]{"base","reduce","estimate","resolve"};ComPtr<ID3D12PipelineState>* psos[]{&basePSO,&reducePSO,&estimatePSO,&resolvePSO};
  for(int i=0;i<4;i++){ComPtr<ID3DBlob> code;auto hr=D3DCompile(flowShader,sizeof(flowShader),nullptr,nullptr,nullptr,entries[i],"cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&error);requireShader(hr,error.Get(),entries[i]);D3D12_COMPUTE_PIPELINE_STATE_DESC p{};p.pRootSignature=root.Get();p.CS={code->GetBufferPointer(),code->GetBufferSize()};require(gpu->CreateComputePipelineState(&p,IID_PPV_ARGS(psos[i]->ReleaseAndGetAddressOf())),"Motion PSO");}
  for(UINT i=0;i<levels;i++){UINT pw=(w+(1u<<i)-1)>>i,ph=(h+(1u<<i)-1)>>i;current[i]=texture(pw,ph,DXGI_FORMAT_R32G32_FLOAT);old[i]=texture(pw,ph,DXGI_FORMAT_R32G32_FLOAT);if(i>=2)flow[i]=texture(pw,ph,DXGI_FORMAT_R16G16B16A16_FLOAT);}
  fullFlow=texture(w,h,DXGI_FORMAT_R16G16B16A16_FLOAT);previousFlow=texture(w,h,DXGI_FORMAT_R16G16B16A16_FLOAT);
 }
 void reset(){history=false;}
 void record(ID3D12GraphicsCommandList* list,ID3D12Resource* color,ID3D12Resource* depth,ID3D12Resource* mv,ID3D12Resource* mask,MotionConstants camera,DXGI_FORMAT depthFormat=DXGI_FORMAT_R24_UNORM_X8_TYPELESS,ID3D12Resource* externalFlow=nullptr,ID3D12Resource* confidence=nullptr){
  slot=0;camera.history=camera.history&&history;Params p{camera,0,width,height,0};
  // Caller owns colour/depth in PIXEL_SHADER_RESOURCE and output guides in COMMON.
  transition(list,color,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(list,depth,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  for(UINT i=0;i<levels;i++){transition(list,current[i].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);transition(list,old[i].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);}
  transition(list,previousFlow.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  ID3D12Resource* inputs[8]{nullptr,nullptr,nullptr,previousFlow.Get(),depth,old[0].Get(),color,confidence};
  if(externalFlow){transition(list,externalFlow,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(list,confidence,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);}ID3D12Resource* outputs[4]{current[0].Get(),nullptr,nullptr,nullptr};
  dispatch(list,basePSO.Get(),p,inputs,outputs,depthFormat);transition(list,current[0].Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  for(UINT i=1;i<levels;i++){p.level=i;p.width=UINT(current[i]->GetDesc().Width);p.height=current[i]->GetDesc().Height;inputs[0]=current[i-1].Get();outputs[0]=current[i].Get();dispatch(list,reducePSO.Get(),p,inputs,outputs,depthFormat);transition(list,current[i].Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);}
  outputs[0]=nullptr;
  for(int i=levels-1;!externalFlow&&i>=2;i--){p.level=i;p.width=UINT(current[i]->GetDesc().Width);p.height=current[i]->GetDesc().Height;p.coarse=i<int(levels)-1;inputs[0]=current[i].Get();inputs[1]=old[i].Get();inputs[2]=p.coarse?flow[i+1].Get():nullptr;outputs[1]=flow[i].Get();transition(list,flow[i].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);dispatch(list,estimatePSO.Get(),p,inputs,outputs,depthFormat);transition(list,flow[i].Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);}
  p.level=0;p.width=width;p.height=height;inputs[0]=current[0].Get();inputs[1]=old[0].Get();inputs[2]=externalFlow?externalFlow:flow[2].Get();p.coarse=externalFlow?2:1;outputs[1]=fullFlow.Get();outputs[2]=mask;outputs[3]=mv;
  for(auto resource:{fullFlow.Get(),mask,mv})transition(list,resource,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  dispatch(list,resolvePSO.Get(),p,inputs,outputs,depthFormat);
  for(auto resource:{mask,mv})transition(list,resource,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON);
  transition(list,fullFlow.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  for(UINT i=0;i<levels;i++)copy(list,current[i].Get(),old[i].Get());copy(list,fullFlow.Get(),previousFlow.Get());
  for(UINT i=2;!externalFlow&&i<levels;i++)transition(list,flow[i].Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
  transition(list,color,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);transition(list,depth,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);if(externalFlow){transition(list,externalFlow,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);transition(list,confidence,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}history=true;
 }
};
