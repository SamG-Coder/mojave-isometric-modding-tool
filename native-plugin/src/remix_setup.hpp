// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
// Included after native interaction helpers.
remix_catalog::Catalog rtxCatalog;bool rtxCatalogReady{};uint32_t rtxBucket{};
std::filesystem::recursive_directory_iterator rtxAssets;uint64_t rtxAssetScanAt{};
std::string rtxPendingScene;std::map<std::string,unsigned> rtxSceneAttempts;
std::string rtxScene,rtxCaptureStatus="Off";uint64_t rtxStableSince{},rtxLastSample{},rtxLastCapture{},rtxKeyUp{};unsigned rtxCaptureRequests{};
void rtxReleaseKey(){if(rtxKeyUp){INPUT i{};i.type=INPUT_KEYBOARD;i.ki.wVk=VK_F24;i.ki.dwFlags=KEYEVENTF_KEYUP;SendInput(1,&i,sizeof(i));rtxKeyUp=0;}}
void rtxTick(){
 auto now=GetTickCount64();if(rtxKeyUp&&now>=rtxKeyUp)rtxReleaseKey();
 if(rtxMode!=2||rtxSessionMode!=2){rtxCaptureStatus=rtxMode!=rtxSessionMode?"Restart required":(rtxMode==0?"Off":"Profile playback");rtxReleaseKey();return;}
 if(!active()){rtxScene.clear();rtxStableSince=0;rtxReleaseKey();return;}
 if(now-rtxLastSample<1000)return;rtxLastSample=now;
 try {
  if(!rtxCatalogReady){rtxCatalogReady=rtxCatalog.open(std::filesystem::path(bridge)/"remix/profile/catalog.jsonl");if(!rtxCatalogReady){rtxCaptureStatus="Cannot write catalogue";return;}}
  auto cell=at<void*>(player(),0x40);auto cellId=at<uint32_t>(cell,0xC);auto pos=at<Vec>(player(),0x30);
  auto key=remix_catalog::sceneKey(cellId,pos.x,pos.y,yaw);
  if(key!=rtxScene){rtxScene=key;rtxStableSince=now;}
  rtxCatalog.add("scene:"+key,"\"kind\":\"scene\",\"cell\":"+std::to_string(cellId)+",\"x\":"+std::to_string(pos.x)+",\"y\":"+std::to_string(pos.y)+",\"z\":"+std::to_string(pos.z)+",\"yaw\":"+std::to_string(yaw)+",\"pitch\":"+std::to_string(pitch)+",\"span\":"+std::to_string(span));
  rtxCatalog.add("profile:1","\"kind\":\"profile\",\"schema\":1,\"runtime\":\"remix-1.5.2\",\"configuration\":\"rtx.conf\",\"asset_source\":\"rtx-remix/captures\",\"compatibility\":\"experimental-unverified\"");
  auto table=global(0x11C54C0);auto buckets=table?at<void**>(table,8):nullptr;auto count=table?at<uint32_t>(table,4):0;
  if(buckets&&count&&count<1000000){unsigned refs=0;for(unsigned step=0;step<256&&refs<128;++step){rtxBucket%=count;auto e=buckets[rtxBucket++];for(unsigned chain=0;e&&chain<128&&refs<128;++chain,e=at<void*>(e,0),++refs){auto ref=at<void*>(e,8);if(!ref)continue;auto type=at<uint8_t>(ref,4);if(type<0x3A||type>0x3C||at<void*>(ref,0x40)!=cell)continue;auto base=at<void*>(ref,0x20);auto render=at<void*>(ref,0x64);if(!base||!render||!at<void*>(render,0x14))continue;auto id=at<uint32_t>(ref,0xC),baseId=at<uint32_t>(base,0xC);auto p=at<Vec>(ref,0x30);rtxCatalog.add("ref:"+std::to_string(cellId)+":"+std::to_string(id),"\"kind\":\"reference\",\"cell\":"+std::to_string(cellId)+",\"reference\":"+std::to_string(id)+",\"base\":"+std::to_string(baseId)+",\"form_type\":"+std::to_string(at<uint8_t>(base,4))+",\"name\":"+remix_catalog::quote(objectName(ref))+",\"x\":"+std::to_string(p.x)+",\"y\":"+std::to_string(p.y)+",\"z\":"+std::to_string(p.z));}}}
  // Confirm exported stages independently of requests; never fabricate success.
  auto captureDir=std::filesystem::path(root).parent_path()/"rtx-remix/captures";std::error_code ec;
  if(std::filesystem::exists(captureDir,ec)){unsigned n=0;for(auto& entry:std::filesystem::directory_iterator(captureDir,ec)){if(++n>4096)break;auto ext=entry.path().extension().string();if(ext==".usd"||ext==".usda"||ext==".usdc"){auto path=entry.path().filename().string();bool newCapture=!rtxCatalog.contains("capture:"+path);rtxCatalog.add("capture:"+path,"\"kind\":\"capture_file\",\"path\":"+remix_catalog::quote(entry.path().string()));if(newCapture&&!rtxPendingScene.empty()&&std::filesystem::file_size(entry.path(),ec)>0){auto age=std::filesystem::file_time_type::clock::now()-std::filesystem::last_write_time(entry.path(),ec);if(!ec&&age<std::chrono::seconds(20)){rtxCatalog.add("captured-scene:"+rtxPendingScene,"\"kind\":\"scene_capture\",\"capture\":"+remix_catalog::quote(path));rtxPendingScene.clear();}}}}}
  // Incrementally index capture assets, not just the native form references.
  // File names retain Remix's mesh/material/texture identifiers; no guessed hashes.
  if(rtxAssets==std::filesystem::recursive_directory_iterator{}&&now-rtxAssetScanAt>=10000&&std::filesystem::exists(captureDir,ec)){
   rtxAssets=std::filesystem::recursive_directory_iterator(captureDir,std::filesystem::directory_options::skip_permission_denied,ec);rtxAssetScanAt=now;
  }
  for(unsigned n=0;n<64&&rtxAssets!=std::filesystem::recursive_directory_iterator{};++n){
   auto entry=*rtxAssets;rtxAssets.increment(ec);if(ec){rtxAssets={};break;}
   if(!entry.is_regular_file(ec))continue;auto relative=entry.path().lexically_relative(captureDir).generic_string();
   rtxCatalog.add("asset:"+relative,"\"kind\":\"captured_asset_file\",\"path\":"+remix_catalog::quote(entry.path().string())+",\"bytes\":"+std::to_string(entry.file_size(ec)));
  }
  DWORD foreground{};GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
  if(foreground==GetCurrentProcessId()&&rtxStableSince&&now-rtxStableSince>=5000&&now-rtxLastCapture>=20000&&rtxCaptureRequests<64&&!rtxCatalog.contains("captured-scene:"+key)&&rtxSceneAttempts[key]<3){
   INPUT i{};i.type=INPUT_KEYBOARD;i.ki.wVk=VK_F24;
   if(SendInput(1,&i,sizeof(i))==1){rtxKeyUp=now+150;rtxLastCapture=now;rtxPendingScene=key;++rtxCaptureRequests;++rtxSceneAttempts[key];rtxCatalog.add("requested:"+key,"\"kind\":\"capture_request\",\"scene\":"+remix_catalog::quote(key));}
  }
  if(!rtxCatalog.flush()){rtxCaptureStatus="Cannot flush catalogue";return;}
  rtxCaptureStatus="Setup: "+std::to_string(rtxCatalog.size())+" records; "+std::to_string(rtxCaptureRequests)+" capture requests";
 }catch(const std::exception&){rtxCaptureStatus="Catalogue I/O failed; profile retained";}
}

std::string rtxStatus(){return rtxCaptureStatus;}
