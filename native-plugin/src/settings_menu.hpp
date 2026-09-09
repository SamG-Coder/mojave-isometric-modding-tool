// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
// Included inside the plugin namespace after native Tile helpers.
namespace settings_menu {
struct Row {std::string name;std::wstring section,key;std::vector<std::wstring> values;std::vector<std::string> labels;size_t selected{};std::wstring original;};
std::vector<Row> rows;void* owner{};int page{};std::string message;
using Update=void(__thiscall*)(void*);Update updateOriginal{};
float get(void* tile,uint32_t id,float fallback){auto v=tileValue(tile,id);return v?at<float>(v,8):fallback;}
void set(void* tile,uint32_t id,float v){if(tile)reinterpret_cast<void(__thiscall*)(void*,uint32_t,float,bool)>(0xA012D0)(tile,id,v,true);}
void text(void* tile,const std::string& s){auto v=tileValue(tile,0xFC4);if(v)reinterpret_cast<void(__thiscall*)(void*,const char*,bool)>(0xA0A300)(v,s.c_str(),true);}
std::string narrow(const std::wstring& v){int n=WideCharToMultiByte(CP_ACP,0,v.c_str(),int(v.size()),nullptr,0,nullptr,nullptr);std::string out(n,0);WideCharToMultiByte(CP_ACP,0,v.c_str(),int(v.size()),out.data(),n,nullptr,nullptr);return out;}
std::filesystem::path pending(){return std::filesystem::path(root)/L"runtime/display-pending.ini";}
std::wstring read(const wchar_t* section,const wchar_t* key,const wchar_t* fallback=L"0"){
 auto value=display_settings::read(pending(),section,key);return value.empty()?display_settings::read(display_settings::prefsPath(),section,key,fallback):value;
}
void select(Row& r,std::wstring value){auto found=std::find(r.values.begin(),r.values.end(),value);if(found==r.values.end()){r.values.push_back(value);r.labels.push_back(narrow(value));r.selected=r.values.size()-1;}else r.selected=size_t(found-r.values.begin());r.original=value;}
void add(std::string name,std::wstring section,std::wstring key,std::vector<std::wstring> values,std::vector<std::string> labels,std::wstring value){Row r{std::move(name),std::move(section),std::move(key),std::move(values),std::move(labels)};select(r,value);rows.push_back(std::move(r));}
void fillResolutions(UINT adapter,std::wstring current){
 auto api=Direct3DCreate9(D3D_SDK_VERSION);std::vector<std::pair<UINT,UINT>> modes;
 if(api){for(UINT i=0;i<api->GetAdapterModeCount(adapter,D3DFMT_X8R8G8B8);++i){D3DDISPLAYMODE m{};if(SUCCEEDED(api->EnumAdapterModes(adapter,D3DFMT_X8R8G8B8,i,&m))&&m.Width>=640&&m.Height>=480)modes.push_back({m.Width,m.Height});}api->Release();}
 std::sort(modes.begin(),modes.end());modes.erase(std::unique(modes.begin(),modes.end()),modes.end());
 Row& r=rows[1];r.values.clear();r.labels.clear();for(auto [w,h]:modes){auto v=std::to_wstring(w)+L" x "+std::to_wstring(h);r.values.push_back(v);r.labels.push_back(narrow(v));}select(r,current);
}
void open(int kind){
 page=kind;message.clear();rows.clear();
 // Snapshot only native children; custom tiles are managed separately.

 if(page==1){
  auto api=Direct3DCreate9(D3D_SDK_VERSION);std::vector<std::wstring> values;std::vector<std::string> names;
  if(api){for(UINT i=0;i<api->GetAdapterCount();i++){D3DADAPTER_IDENTIFIER9 info{};if(SUCCEEDED(api->GetAdapterIdentifier(i,0,&info))){values.push_back(std::to_wstring(i));names.push_back(info.Description);}}api->Release();}
  add("Graphics adapter",L"Display",L"iAdapter",values,names,read(L"Display",L"iAdapter"));
  add("Resolution",L"Display",L"resolution",{}, {},L"");fillResolutions(UINT(_wtoi(rows[0].values[rows[0].selected].c_str())),read(L"Display",L"iSize W",L"1024")+L" x "+read(L"Display",L"iSize H",L"768"));
  for(auto& o:display_settings::options())add(o.label,o.section,o.key,o.values,o.labels,read(o.section,o.key));
 }else{
  if(experimental_rendering::enabled)message=std::string("DLSS: ")+renderer_bridge::status()+". Appearance changes need restart.";
  add("Projection",L"",L"projection",{L"1",L"0"},{"Isometric","Perspective"},orthographic?L"1":L"0");
  std::vector<std::wstring> zoom;std::vector<std::string> zoomText;for(int n=500;n<=4000;n+=250){zoom.push_back(std::to_wstring(n));zoomText.push_back(std::to_string(n));}add("Camera zoom span",L"",L"span",zoom,zoomText,std::to_wstring(int(span)));
  std::vector<std::wstring> angles;std::vector<std::string> angleText;for(int n=20;n<=80;n+=5){angles.push_back(std::to_wstring(n));angleText.push_back(std::to_string(n)+" degrees");}add("Camera pitch",L"",L"pitch",angles,angleText,std::to_wstring(int(desiredPitch)));
  add("Rotation speed",L"",L"rotation",{L"0.15",L"0.25",L"0.35",L"0.5",L"0.75"},{"Very slow","Slow","Normal","Fast","Very fast"},([]{char b[32];sprintf_s(b,"%.2g",rotationSpeed);return std::wstring(b,b+strlen(b));})());
  add("Alt aim line",L"",L"aim_line",{L"0",L"1"},{"Off","On"},showAimLine?L"1":L"0");
  add("Damage numbers",L"",L"damage",{L"0",L"1"},{"Off","On"},showDamageNumbers?L"1":L"0");
  add("Automatic distant aiming",L"",L"ads",{L"0",L"1"},{"Off","On"},automaticADS?L"1":L"0");
  add("Start isometric after loading",L"",L"auto",{L"0",L"1"},{"Off","On"},autoEnable?L"1":L"0");
  if(experimental_rendering::enabled){
  add("Experimental RTX Remix",L"",L"rtx_remix",{L"0",L"1",L"2"},{"Off","On","Setup"},std::to_wstring(rtxMode));
  add("Experimental DLSS 5",L"",L"dlss5",{L"0",L"2"},{"Off","On"},std::to_wstring(GetPrivateProfileIntW(L"bridge",L"mode",0,(std::filesystem::path(bridge)/L"dlss5/bridge.ini").c_str())));
  add("DLSS guide view",L"",L"dlss_debug",{L"0",L"1",L"2"},{"Off","Motion","History mask"},std::to_wstring(renderer_bridge::debugView));
  for(const auto& option:dlss_appearance::options()){
   std::vector<std::wstring> values;std::vector<std::string> labels;
   for(int n=option.low;n<=option.high;n+=option.step){
    wchar_t value[32];if(option.labels.empty())swprintf_s(value,L"%.2f",n/100.0);else swprintf_s(value,L"%d",n);
    values.emplace_back(value);labels.push_back(option.labels.empty()?narrow(value):option.labels[n-option.low]);
   }
   auto current=dlss_appearance::selected(std::filesystem::path(bridge)/L"dlss5",option);
   // Equivalent INI numbers (1, 1.0, 1.00) select the existing native arrow value.
   for(const auto& value:values)if(std::abs(wcstof(value.c_str(),nullptr)-wcstof(current.c_str(),nullptr))<.0001f){current=value;break;}
   add(option.label,dlss_appearance::section,option.key,std::move(values),std::move(labels),current);
  }
  }
 }

}
bool apply(){
 if(page==1){
  auto selected=[&](const wchar_t* key){for(auto& r:rows)if(r.key==key)return r.values[r.selected];return std::wstring();};
  auto api=Direct3DCreate9(D3D_SDK_VERSION);
  if(!api){message="Display device unavailable; changes not saved";return false;}
  UINT adapter=UINT(_wtoi(selected(L"iAdapter").c_str()));D3DDISPLAYMODE mode{};
  bool windowed=selected(L"bFull Screen")==L"0";
  auto sample=D3DMULTISAMPLE_TYPE(_wtoi(selected(L"iMultiSample").c_str()));
  bool supported=adapter<api->GetAdapterCount()&&SUCCEEDED(api->GetAdapterDisplayMode(adapter,&mode))&&SUCCEEDED(api->CheckDeviceMultiSampleType(adapter,D3DDEVTYPE_HAL,mode.Format,windowed,sample,nullptr));
  unsigned modeWidth{},modeHeight{};auto resolution=selected(L"resolution");
  supported=supported&&swscanf_s(resolution.c_str(),L"%u x %u",&modeWidth,&modeHeight)==2&&modeWidth>=640&&modeHeight>=480&&modeWidth<=16384&&modeHeight<=16384;
  if(supported&&!windowed){supported=false;for(UINT i=0;i<api->GetAdapterModeCount(adapter,mode.Format);++i){D3DDISPLAYMODE m{};if(SUCCEEDED(api->EnumAdapterModes(adapter,mode.Format,i,&m))&&m.Width==modeWidth&&m.Height==modeHeight){supported=true;break;}}}
  api->Release();if(!supported){message="Unsupported display mode or anti-aliasing";return false;}
  auto tmp=pending();tmp+=L".tmp";std::error_code ec;
  std::filesystem::remove(tmp,ec);ec.clear();
  if(std::filesystem::exists(pending()))std::filesystem::copy_file(pending(),tmp,std::filesystem::copy_options::overwrite_existing,ec);
  if(ec){message="Could not save settings. Changes remain unapplied.";return false;}
  bool changed=false;
  auto write=[&](const wchar_t* section,const wchar_t* key,const std::wstring& v){return WritePrivateProfileStringW(section,key,v.c_str(),tmp.c_str())!=0;};
  for(auto& r:rows){auto v=r.values[r.selected];if(v==r.original)continue;changed=true;
   if(r.key==L"resolution"){unsigned w{},h{};if(swscanf_s(v.c_str(),L"%u x %u",&w,&h)!=2||w<640||h<480||w>16384||h>16384){message="Select a valid resolution.";return false;}if(!write(L"Display",L"iSize W",std::to_wstring(w))||!write(L"Display",L"iSize H",std::to_wstring(h)))return false;}
   else if(!write(r.section.c_str(),r.key.c_str(),v))return false;
   if(r.key==L"iAdapter"){auto name=r.labels[r.selected];if(!write(L"Display",L"sD3DDevice",std::wstring(name.begin(),name.end())))return false;}
  }
  if(changed){WritePrivateProfileStringW(nullptr,nullptr,nullptr,tmp.c_str());if(!MoveFileExW(tmp.c_str(),pending().c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){message="Could not save display settings.";return false;}}
  if(!changed)std::filesystem::remove(tmp,ec);
  message=changed?"Saved. Applies next time you start the game.":"No new display changes.";
 }else{
  int dlssRequested=0,remixRequested=rtxMode;
  bool appearanceChanged=false;
  if(experimental_rendering::enabled){
  for(auto& r:rows)if(r.key==L"dlss_debug"){renderer_bridge::debugView=_wtoi(r.values[r.selected].c_str());WritePrivateProfileStringW(L"bridge",L"debug_view",r.values[r.selected].c_str(),(std::filesystem::path(bridge)/L"dlss5/bridge.ini").c_str());}
  for(auto& r:rows){if(r.key==L"dlss5")dlssRequested=_wtoi(r.values[r.selected].c_str());if(r.key==L"rtx_remix")remixRequested=_wtoi(r.values[r.selected].c_str());}
  if(dlssRequested&&remixRequested){message="Choose DLSS 5 or RTX Remix, then restart.";return false;}
  if(dlssRequested&&!std::filesystem::exists(std::filesystem::path(bridge)/L"dlss5/candidate/host64/MojaveIsoNeuralHost.exe")){message="DLSS 5 runtime is not installed.";return false;}
  for(auto& r:rows)if(r.key==L"rtx_remix"&&r.values[r.selected]==L"1"&&!std::filesystem::exists(std::filesystem::path(bridge)/"remix/profile/catalog.jsonl")){message="Run Setup in a loaded game to create a profile first.";return false;}
  auto appearanceFile=dlss_appearance::preferences(std::filesystem::path(bridge)/L"dlss5");
  auto appearanceTemp=appearanceFile;appearanceTemp+=L".tmp";
  for(auto& r:rows)if(r.section==dlss_appearance::section&&r.values[r.selected]!=r.original)appearanceChanged=true;
  if(appearanceChanged){
   // Write the complete page snapshot atomically; preserve the previous file on failure.
   for(auto& r:rows)if(r.section==dlss_appearance::section&&!WritePrivateProfileStringW(dlss_appearance::section,r.key.c_str(),r.values[r.selected].c_str(),appearanceTemp.c_str())){message="Could not save DLSS appearance settings.";return false;}
   WritePrivateProfileStringW(nullptr,nullptr,nullptr,appearanceTemp.c_str());
   if(!MoveFileExW(appearanceTemp.c_str(),appearanceFile.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){message="Could not save DLSS appearance settings.";return false;}
  }
  if(!WritePrivateProfileStringW(L"bridge",L"mode",std::to_wstring(dlssRequested).c_str(),(std::filesystem::path(bridge)/L"dlss5/bridge.ini").c_str())){message="Could not save DLSS setting.";return false;}
  }
  for(auto& r:rows){auto v=r.values[r.selected];float n=wcstof(v.c_str(),nullptr);if(r.key==L"projection")orthographic=n!=0;else if(r.key==L"span")span=n;else if(r.key==L"pitch")desiredPitch=n;else if(r.key==L"rotation")rotationSpeed=n;else if(r.key==L"aim_line")showAimLine=n!=0;else if(r.key==L"damage")showDamageNumbers=n!=0;else if(r.key==L"ads")automaticADS=n!=0;else if(r.key==L"auto")autoEnable=n!=0;else if(r.key==L"rtx_remix")rtxMode=std::clamp(int(n),0,2);}
  saveSettings(true);message=appearanceChanged||dlssRequested!=renderer_bridge::mode?"Saved. Restart the game for DLSS 5.":rtxMode!=rtxSessionMode?"Saved. Restart for RTX mode; first setup installs runtime.":"Isometric settings applied.";
 }
 for(auto& r:rows)r.original=r.values[r.selected];return true;
}

// Real StartMenu option objects and ListBox rows. Native templates own layout,
// focus, arrows, scrolling and colours; no overlay XML or input interception.
struct NativeOption {
 uintptr_t vtable{0x107704C}; const char* name{}; void(*followup)(){};uint32_t flags{};
 const char* templateName{"lb_toggle_template"};uint32_t current{},initial{},type{},count{},width{55};
 const char** names{};void(*changed)(NativeOption*){};
};
static_assert(sizeof(NativeOption)==0x30);
struct Binding {NativeOption option;size_t row{};std::string caption;std::vector<const char*> names;};
std::vector<std::unique_ptr<Binding>> bindings;
NativeOption isoEntry,applyEntry,cancelEntry;
void* nativeMenu{};void* isoTile{};void* applyTile{};
void(*displayFollowup)(){};
void* insert(void* menu,int list,NativeOption* option,const char* templ){
 return reinterpret_cast<void*(__thiscall*)(void*,void*,const char*,void*,const char*)>(0x7D6FF0)(static_cast<char*>(menu)+list,option,option->name,nullptr,templ);
}
void notify(const std::string& value){if(applyTile)text(applyTile,value);}
void __cdecl selection(NativeOption* option){
 for(auto& b:bindings)if(&b->option==option){rows[b->row].selected=std::min(size_t(option->current),rows[b->row].values.size()-1);return;}
}
void __cdecl applyNative(){
 if(apply()){for(auto& b:bindings)b->option.initial=b->option.current;}
 notify(message.empty()?"Unable to save settings":message);
}
void __cdecl cancelNative(){
 for(auto& b:bindings){auto& r=rows[b->row];select(r,r.original);b->option.current=uint32_t(r.selected);reinterpret_cast<void(__thiscall*)(void*,void*,void*)>(0x7D4CE0)(nativeMenu,&b->option,nullptr);}
 notify("Changes cancelled");
}
void action(NativeOption& o,const char* name,void(*fn)()){
 o={};o.vtable=0x1076EE0;o.name=name;o.followup=fn;auto tile=insert(nativeMenu,0x114,&o,"lb_item_template");if(&o==&applyEntry)applyTile=tile;
}
bool nativeAlreadyHas(const std::wstring& key){
 return key==L"iTexMipMapSkip"||key==L"fLODFadeOutMultObjects"||key==L"fLODFadeOutMultItems"||key==L"fLODFadeOutMultActors"||key==L"fGrassStartFadeDistance"||key==L"fTreeLoadDistance"||key==L"fShadowLODStartFade"||key==L"fLightLODStartFade"||key==L"fSpecularLODStartFade";
}
void* child(void* tile,const char* name){
 for(auto node=at<void*>(tile,4);node;node=at<void*>(node,0)){
  auto t=at<void*>(node,8);auto text=t?at<const char*>(t,0x20):nullptr;if(text&&!strcmp(text,name))return t;
 }return nullptr;
}
float labelEnd=0,valueSpacing=55;
void alignRows(){
 if(!nativeMenu||bindings.empty())return;
 auto list=static_cast<char*>(nativeMenu)+0x114;bool found=false;
 auto changed=[](void* tile,uint32_t id,float value){if(std::abs(get(tile,id,-1)-value)>.25f)set(tile,id,value);};
 // Walk the current native list, rather than retain tiles across submenu rebuilds.
 for(auto node=list+4;node;node=at<char*>(node,4)){
  auto item=at<void*>(node,0);if(!item)continue;auto option=at<NativeOption*>(item,4);
  auto binding=std::find_if(bindings.begin(),bindings.end(),[&](auto& b){return &b->option==option;});if(binding==bindings.end())continue;
  auto tile=at<void*>(item,0);auto label=child(tile,"ListItemText"),value=child(tile,"lb_toggle_value");
  if(!label||!value)continue;found=true;
  labelEnd=std::max(labelEnd,get(label,0xFA1,10)+get(label,0xFB1,0));
  // user4 is distance from the value centre to each arrow, not box width.
  float spacing=std::max(55.f,get(value,0xFB1,0)*.5f+12.f);
  changed(tile,0x1008,spacing);valueSpacing=std::max(valueSpacing,spacing);
 }
 if(!found)return;
 static const auto centerTrait=reinterpret_cast<uint32_t(__cdecl*)(const char*,uint32_t)>(0xA00940)("_center_x",uint32_t(-1));
 float center=std::max(560.f,labelEnd+valueSpacing+33.f);
 changed(at<void*>(nativeMenu,4),centerTrait,center);
 changed(at<void*>(list,12),0xFB1,std::max(880.f,center+valueSpacing+33.f));
}
void appendRows(){
 bindings.clear();labelEnd=0;valueSpacing=55;
 auto list=static_cast<char*>(nativeMenu)+0x114;auto container=at<void*>(list,12);set(container,0xFB1,880);set(container,0xFA1,330);
 for(size_t i=0;i<rows.size();++i){auto& r=rows[i];if(page==1&&nativeAlreadyHas(r.key))continue;
  auto b=std::make_unique<Binding>();b->row=i;b->caption=r.name+(page==1?" *":"");
  for(auto& label:r.labels)b->names.push_back(label.c_str());
  auto& o=b->option;o.name=b->caption.c_str();o.current=o.initial=uint32_t(r.selected);o.count=uint32_t(r.values.size());o.names=b->names.data();o.changed=selection;
  auto tile=insert(nativeMenu,0x114,&o,o.templateName);
  if(tile){set(tile,0x1008,float(o.width));reinterpret_cast<void(__thiscall*)(void*,void*,void*)>(0x7D4CE0)(nativeMenu,&o,tile);}
  bindings.push_back(std::move(b));
 }
 action(applyEntry,page==1?"Apply display changes (restart required)":"Apply isometric changes",applyNative);
 action(cancelEntry,"Cancel unapplied changes",cancelNative);
 alignRows();
 auto first=at<void*>(list,4);if(first){auto tile=at<void*>(first,0);auto vt=at<uintptr_t*>(list,0);reinterpret_cast<bool(__thiscall*)(void*,void*)>(vt[0])(list,tile);reinterpret_cast<void(__thiscall*)(void*)>(vt[4])(list);}
}
void __cdecl displayOpen(){displayFollowup();open(1);appendRows();}
void __cdecl isoOpen(){
 reinterpret_cast<void(__thiscall*)(void*,bool)>(0x7D74F0)(static_cast<char*>(nativeMenu)+0xB4,false);
 reinterpret_cast<void(__thiscall*)(void*,uint32_t)>(0x7D42B0)(nativeMenu,0);
 open(2);appendRows();
}
void refresh(void* menu){
 nativeMenu=menu;owner=at<void*>(menu,4);
 auto list=static_cast<char*>(menu)+0xB4;bool present=false;
 for(auto node=list+4;node;node=at<char*>(node,4)){
  auto item=at<void*>(node,0);if(!item)continue;auto o=at<NativeOption*>(item,4);if(!o)continue;
  if(o==&isoEntry)present=true;
  if(o->followup==reinterpret_cast<void(*)()>(0x7D0D70)){displayFollowup=o->followup;o->followup=displayOpen;}
 }
 if(!present&&at<void*>(list,12)){
  isoEntry={};isoEntry.vtable=0x1076EE0;isoEntry.name="Isometric";isoEntry.flags=4;isoEntry.followup=isoOpen;
  isoTile=insert(menu,0xB4,&isoEntry,"lb_item_template");log("Isometric entry registered in native Settings list");
 }
}
void __fastcall update(void* m,void*){updateOriginal(m);refresh(m);alignRows();}
void install(){void* prior{};if(replaceSlot(0x1076D1C+0x2C,reinterpret_cast<void*>(update),prior))updateOriginal=reinterpret_cast<Update>(prior);log("Native Settings list integration installed");}
}
void installSettingsMenu(){settings_menu::install();}
