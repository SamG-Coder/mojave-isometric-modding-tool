// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
// D3D12 execution adapter. Shader algorithms are loaded from a private runtime
// file prepared from the user's Lumenite installation, never embedded here.
class LumeniteRuntime {
 ComPtr<ID3D12Device> gpu;ComPtr<ID3D12RootSignature> root;
 ComPtr<ID3D12DescriptorHeap> srv,rtv;ComPtr<ID3D12Resource> textures[15];
 struct Pass {const char* entry;UINT output,second;ComPtr<ID3D12PipelineState> pso;};
 std::vector<Pass> passes;ComPtr<ID3D12PipelineState> downsample;
 UINT width{},height{},srvStep{},rtvStep{},descriptor{},frame{};bool initialized{};
 D3D12_CPU_DESCRIPTOR_HANDLE cpu(bool target,UINT index){auto h=target?rtv->GetCPUDescriptorHandleForHeapStart():srv->GetCPUDescriptorHandleForHeapStart();h.ptr+=index*(target?rtvStep:srvStep);return h;}
 void barrier(ID3D12GraphicsCommandList* list,ID3D12Resource* resource,UINT mip,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={resource,mip,from,to};list->ResourceBarrier(1,&b);}
 ComPtr<ID3D12PipelineState> compile(const std::string& source,const char* entry,ID3DBlob* vs,const std::vector<DXGI_FORMAT>& formats){
  std::string w=std::to_string(width),h=std::to_string(height);D3D_SHADER_MACRO defines[]{{"BUFFER_WIDTH",w.c_str()},{"BUFFER_HEIGHT",h.c_str()},{nullptr,nullptr}};
  ComPtr<ID3DBlob> shader,error;auto hr=D3DCompile(source.data(),source.size(),"Private Lumenite QuantMotion",defines,nullptr,entry,"ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&shader,&error);requireShader(hr,error.Get(),entry);
  D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=root.Get();p.VS={vs->GetBufferPointer(),vs->GetBufferSize()};p.PS={shader->GetBufferPointer(),shader->GetBufferSize()};p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.RasterizerState.DepthClipEnable=TRUE;p.SampleMask=UINT_MAX;p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=UINT(formats.size());p.SampleDesc.Count=1;p.BlendState.IndependentBlendEnable=TRUE;
  p.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;p.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};p.DepthStencilState.BackFace=p.DepthStencilState.FrontFace;
  for(auto& rt:p.BlendState.RenderTarget){rt.RenderTargetWriteMask=15;rt.SrcBlend=rt.SrcBlendAlpha=D3D12_BLEND_ONE;rt.DestBlend=rt.DestBlendAlpha=D3D12_BLEND_ZERO;rt.BlendOp=rt.BlendOpAlpha=D3D12_BLEND_OP_ADD;rt.LogicOp=D3D12_LOGIC_OP_NOOP;}
  for(UINT i=0;i<formats.size();i++)p.RTVFormats[i]=formats[i];ComPtr<ID3D12PipelineState> result;require(gpu->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&result)),"Lumenite PSO");return result;
 }
 void draw(ID3D12GraphicsCommandList* list,ID3D12PipelineState* pso,UINT output,UINT second=0,UINT mip=0,bool reducing=false){
  auto resource=textures[output].Get();auto desc=resource->GetDesc();UINT w=std::max(1u,UINT(desc.Width)>>mip),h=std::max(1u,desc.Height>>mip);
  barrier(list,resource,mip,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
  if(second)barrier(list,textures[second].Get(),0,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
  UINT base=descriptor;descriptor+=15;
  for(UINT i=0;i<15;i++){
   auto input=(i==output||(second&&i==second))?nullptr:textures[i].Get();if(reducing)input=i==0?resource:nullptr;
   D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=input?input->GetDesc().Format:DXGI_FORMAT_R16G16B16A16_FLOAT;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MostDetailedMip=reducing&&i==0?mip-1:0;d.Texture2D.MipLevels=reducing?1:input?input->GetDesc().MipLevels:1;gpu->CreateShaderResourceView(input,&d,cpu(false,base+i));
  }
  D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=desc.Format;view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;view.Texture2D.MipSlice=mip;gpu->CreateRenderTargetView(resource,&view,cpu(true,0));if(second)gpu->CreateRenderTargetView(textures[second].Get(),nullptr,cpu(true,1));
  ID3D12DescriptorHeap* heaps[]{srv.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootSignature(root.Get());list->SetPipelineState(pso);auto table=srv->GetGPUDescriptorHandleForHeapStart();table.ptr+=base*srvStep;list->SetGraphicsRootDescriptorTable(0,table);list->SetGraphicsRoot32BitConstant(1,frame,0);
  D3D12_VIEWPORT vp{0,0,float(w),float(h),0,1};D3D12_RECT rect{0,0,LONG(w),LONG(h)};list->RSSetViewports(1,&vp);list->RSSetScissorRects(1,&rect);auto target=cpu(true,0);list->OMSetRenderTargets(second?2:1,&target,TRUE,nullptr);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
  barrier(list,resource,mip,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);if(second)barrier(list,textures[second].Get(),0,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
 }
public:
 ID3D12Resource* flow()const{return textures[1].Get();}
 ID3D12Resource* confidence()const{return textures[2].Get();}
 void init(ID3D12Device* device,UINT w,UINT h,const std::filesystem::path& directory){
  gpu=device;width=w;height=h;std::ifstream file(directory/L"lumenite/QuantMotion.hlsl",std::ios::binary);if(!file)throw std::runtime_error("Lumenite runtime missing; run desktop/setup-lumenite.py with the supplied package");std::string source((std::istreambuf_iterator<char>(file)),{});
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=512;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;require(gpu->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srv)),"Lumenite SRVs");hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=2;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;require(gpu->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"Lumenite RTVs");srvStep=gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);rtvStep=gpu->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,15,0,0,0};D3D12_ROOT_PARAMETER params[2]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[0].DescriptorTable={1,&range};params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[1].Constants={0,0,1};D3D12_STATIC_SAMPLER_DESC samplers[2]{};
  for(UINT i=0;i<2;i++){samplers[i].Filter=i?D3D12_FILTER_MIN_MAG_MIP_LINEAR:D3D12_FILTER_MIN_MAG_MIP_POINT;samplers[i].AddressU=samplers[i].AddressV=samplers[i].AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;samplers[i].ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;samplers[i].MaxLOD=D3D12_FLOAT32_MAX;samplers[i].ShaderRegister=i;samplers[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
  D3D12_ROOT_SIGNATURE_DESC rootDesc{};rootDesc.NumParameters=2;rootDesc.pParameters=params;rootDesc.NumStaticSamplers=2;rootDesc.pStaticSamplers=samplers;ComPtr<ID3DBlob> signature,error;require(D3D12SerializeRootSignature(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&error),"Lumenite root");require(gpu->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)),"Lumenite root create");
  UINT scales[]{1,8,8,1,1,128,64,64,32,32,16,16,8,8,8};
  for(UINT i=1;i<15;i++){D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=std::max(1u,w/scales[i]);d.Height=std::max(1u,h/scales[i]);d.DepthOrArraySize=1;d.MipLevels=1;if(i==3||i==4){UINT n=std::min(w,h);while(n>1&&d.MipLevels<8){n>>=1;d.MipLevels++;}}d.Format=(i==2||i==3||i==4||i==14)?DXGI_FORMAT_R16_FLOAT:DXGI_FORMAT_R16G16_FLOAT;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;require(gpu->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&textures[i])),"Lumenite texture");}
  std::string sw=std::to_string(w),sh=std::to_string(h);D3D_SHADER_MACRO defines[]{{"BUFFER_WIDTH",sw.c_str()},{"BUFFER_HEIGHT",sh.c_str()},{nullptr,nullptr}};ComPtr<ID3DBlob> vs;auto hr=D3DCompile(source.data(),source.size(),"Private Lumenite QuantMotion",defines,nullptr,"MojaveVS","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vs,&error);requireShader(hr,error.Get(),"Lumenite VS");
  passes={{"PS_PackFeatures",3,0},{"PS_ComputeFlow128",5,0},{"PS_UpscaleFlow64",6,0},{"PS_MedianPass64",7,0},{"PS_UpscaleFlow32",8,0},{"PS_MedianPass32",9,0},{"PS_UpscaleFlow16",10,0},{"PS_MedianPass16",11,0},{"PS_UpscaleFlow8",12,0},{"PS_MedianPass8",1,0},{"PS_Confidence",2,0},{"PS_ATrousPassA",12,0},{"PS_ATrousPassB",1,0},{"PS_StoreFlow",13,14},{"PS_StoreLuma",4,0}};
  for(auto& pass:passes){std::vector<DXGI_FORMAT> formats{textures[pass.output]->GetDesc().Format};if(pass.second)formats.push_back(textures[pass.second]->GetDesc().Format);pass.pso=compile(source,pass.entry,vs.Get(),formats);}downsample=compile(source,"MojaveDownsample",vs.Get(),{DXGI_FORMAT_R16_FLOAT});
 }
 void record(ID3D12GraphicsCommandList* list,ID3D12Resource* color,bool reset){
  textures[0]=color;descriptor=0;if(reset)frame=0;
  if(!initialized){for(UINT i=1;i<15;i++)transition(list,textures[i].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);initialized=true;}
  for(auto& pass:passes){draw(list,pass.pso.Get(),pass.output,pass.second);if(pass.output==3||pass.output==4)for(UINT mip=1;mip<textures[pass.output]->GetDesc().MipLevels;mip++)draw(list,downsample.Get(),pass.output,0,mip,true);}
  frame++;
 }
};
