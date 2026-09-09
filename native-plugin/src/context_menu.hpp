// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
// Included inside the plugin namespace, after native HUD and interaction helpers.
struct ContextState {
 void* owner{};void* tile{};uint32_t target{},cell{};Vec point{};
 ScreenPoint anchor{};context_menu::Layout layout{};std::vector<context_menu::Row> rows;
 std::string title;
} context;
void closeContext(){
 contextOpen=false;
 auto hud=global(0x11D96C0);auto rootTile=hud?at<void*>(hud,4):nullptr;
 if(context.owner==rootTile&&context.tile)settings_menu::set(context.tile,0xFA3,0);
 context.rows.clear();
}
void resetContext(){closeContext();context={};}
bool validContext(){
 if(!active()||!player()||!at<void*>(player(),0x40)||at<uint32_t>(at<void*>(player(),0x40),0xC)!=context.cell)return false;
 if(!context.target)return true;
 auto ref=reference(context.target);return ref&&!(at<uint32_t>(ref,8)&0x820)&&at<void*>(ref,0x40)==at<void*>(player(),0x40);
}
void openContext(){
 closeContext();stop();Vec hit{};void* object{};
 if(!active()||!pick(cursorX,cursorY,hit,object,true))return;
 auto ref=parentReference(object);if(ref==player())ref=nullptr;
 if(ref&&at<void*>(ref,0x40)!=at<void*>(player(),0x40))ref=nullptr;
 context.target=ref?at<uint32_t>(ref,0xC):0;context.point=hit;
 context.cell=at<uint32_t>(at<void*>(player(),0x40),0xC);context.anchor={cursorX/width,cursorY/height};
 unsigned kind=ref&&at<void*>(ref,0x20)?at<uint8_t>(at<void*>(ref,0x20),4):0;
 context.rows=context_menu::actions(kind,(kind==0x2A||kind==0x2B)&&!combatActor(ref),combatActor(ref),ref!=nullptr);
 context.title=ref?objectName(ref):"Ground";contextOpen=true;updateContext();
}
void updateContext(){
 auto hud=global(0x11D96C0);auto rootTile=hud?at<void*>(hud,4):nullptr;
 if(rootTile!=context.owner){context.owner=rootTile;context.tile=nullptr;if(contextOpen)contextOpen=false;}
 if(!rootTile)return;
 if(!contextOpen){if(context.tile)settings_menu::set(context.tile,0xFA3,0);return;}
 if(!validContext()){closeContext();return;}
 if(auto ref=reference(context.target)){
  auto base=at<void*>(ref,0x20);unsigned kind=base?at<uint8_t>(base,4):0;
  context.rows=context_menu::actions(kind,(kind==0x2A||kind==0x2B)&&!combatActor(ref),combatActor(ref),true);
 }
 if(!context.tile)context.tile=reinterpret_cast<void*(__thiscall*)(void*,const char*)>(0xA01B00)(rootTile,R"(menus\MojaveIso\context.xml)");
 if(!context.tile){closeContext();note="Context menu XML unavailable";return;}
 auto ui=nativeUIExtent(rootTile);if(ui.x<=0||ui.y<=0){closeContext();return;}
 auto child=[&](const char* n){return settings_menu::child(context.tile,n);};
 auto textStyle=[&](void* t){auto source=at<void*>(hud,0xA8);
  for(uint32_t id:{0xFB9u,0xFB2u,0xFB3u,0xFB4u,0xFB8u})if(auto v=tileValue(source,id))settings_menu::set(t,id,at<float>(v,8));
 };
 auto title=child("Title");settings_menu::text(title,"[ "+context.title+" ]");textStyle(title);
 float textHeight=std::max(24.f,settings_menu::get(title,0xFB0,24)),textWidth=settings_menu::get(title,0xFB1,240);
 for(size_t i=0;i<context.rows.size();++i){auto t=child(("Row"+std::to_string(i)).c_str());textStyle(t);settings_menu::text(t,"  "+context.rows[i].label);textHeight=std::max(textHeight,settings_menu::get(t,0xFB0,24));textWidth=std::max(textWidth,settings_menu::get(t,0xFB1,240));}
 context.layout=context_menu::layout({context.anchor.x*ui.x,context.anchor.y*ui.y},ui,context.rows.size(),textHeight,textWidth);
 auto& l=context.layout;UITransform transform{width,height,ui.x,ui.y};int selected=l.hit(transform.toUI({cursorX,cursorY}));
 settings_menu::set(context.tile,0xFA1,l.x);settings_menu::set(context.tile,0xFA2,l.y);settings_menu::set(context.tile,0xFB1,l.width);settings_menu::set(context.tile,0xFB0,l.height());
 settings_menu::set(child("Background"),0xFB1,l.width);settings_menu::set(child("Background"),0xFB0,l.height());
 for(size_t i=0;i<6;++i){auto t=child(("Row"+std::to_string(i)).c_str());settings_menu::set(t,0xFA3,i<context.rows.size()?1.f:0.f);if(i>=context.rows.size())continue;
  settings_menu::text(t,(selected==int(i)?"> ":"  ")+context.rows[i].label);settings_menu::set(t,0xFA2,l.header+float(i)*l.rowHeight);settings_menu::set(t,0xFA9,selected==int(i)?255.f:190.f);
 }
 settings_menu::set(context.tile,0xFA3,1);
}
bool contextInput(bool click){
 if(!validContext()||(GetAsyncKeyState(VK_ESCAPE)&0x8000)){closeContext();return true;}
 updateContext();if(!click||!contextOpen)return true;
 auto ui=nativeUIExtent(context.owner);if(ui.x<=0||ui.y<=0){closeContext();return true;}
 int index=context.layout.hit(UITransform{width,height,ui.x,ui.y}.toUI({cursorX,cursorY}));
 if(index<0||size_t(index)>=context.rows.size()){closeContext();return true;}
 auto action=context.rows[index].action;auto id=context.target;auto point=context.point;auto ref=reference(id);closeContext();
 switch(action){
 case context_menu::Action::Activate:if(interactable(ref))nearbyDestination(id);break;
 case context_menu::Action::Attack:if(combatActor(ref))attackTarget(ref,bodyPoint(ref));break;
 case context_menu::Action::Vats:if(combatActor(ref)){stop();facePoint(bodyPoint(ref));run("TapControl 16");note="Opening native VATS";}break;
 case context_menu::Action::Move:{stop();Vec floor{};if(length(point-at<Vec>(player(),0x30))<=6000&&groundProbe(point,floor))planDestination(floor,0);else note="No walkable ground at this point";break;}
 case context_menu::Action::Stop:stop();break;
 case context_menu::Action::Cancel:break;
 }
 return true;
}
