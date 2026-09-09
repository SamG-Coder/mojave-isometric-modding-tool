// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#include "../src/camera_math.hpp"
#include <iostream>
#include <cstdlib>
using namespace engine;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<"\n";std::exit(1);}}
int main(){
    unsigned tests=0;
    for(bool ortho:{false,true})for(float yaw:{0.f,45.f,179.f,270.f})for(float pitch:{20.f,50.f,80.f}){
        float y=yaw*.01745329252f,p=pitch*.01745329252f;
        CameraSample c;c.position={-71000,400,9000};c.direction={sinf(y)*cosf(p),cosf(y)*cosf(p),-sinf(p)};
        c.right={cosf(y),-sinf(y),0};c.up=cross(c.right,c.direction);
        c.frustum=ortho?Frustum{-700,900,500,-400,5,24000,true,{}}:Frustum{-.6f,.8f,.5f,-.4f,5,24000,false,{}};
        c.x=32;c.y=18;c.width=1600;c.height=900;c.valid=true;
        for(int ix=0;ix<7;ix++)for(int iy=0;iy<7;iy++){
            float x=40+ix*240.f,y=30+iy*140.f;Vec origin,ray;check(c.ray(x,y,origin,ray),"Valid pixel ray rejected");
            float sx,sy;check(c.project(origin+ray*2000,sx,sy),"Visible world point rejected");
            check(std::abs(sx-x)<.03f&&std::abs(sy-y)<.03f,"Pixel and marker disagree after orbit or viewport offset");++tests;
        }
        if(ortho){
            auto planes=orthographicPlanes(c.position,c.direction,c.up,c.right,c.frustum);
            Vec mid=c.position+c.direction*1000;
            check(intersects(planes,mid+c.right*950,60),"Partially visible right-edge sphere rejected");
            check(intersects(planes,mid+c.up*(-450),60),"Partially visible bottom-edge sphere rejected");
            check(!intersects(planes,mid+c.up*(-470),60),"Fully outside bottom-edge sphere retained");
            check(intersects(planes,mid+c.up*(-460),60),"Tangent sphere rejected");
            auto inside=[&](Vec point){for(auto plane:planes.plane)if(dot(plane.normal,point)-plane.offset<-.03f)return false;return true;};
            for(float depth:{5.f,1000.f,24000.f})for(float a:{-700.f,100.f,900.f})for(float b:{-400.f,0.f,500.f})
                check(inside(c.position+c.direction*depth+c.right*a+c.up*b),"Full orthographic camera volume was culled");
            check(!inside(c.position+c.direction*1000+c.up*(-410.f)),"Below-bottom point accepted");
            check(!inside(c.position+c.direction*1000+c.up*510.f),"Above-top point accepted");
            check(!inside(c.position+c.direction*1000+c.right*910.f),"Beyond-right point accepted");
            check(!inside(c.position+c.direction*1000+c.right*(-710.f)),"Beyond-left point accepted");
            check(!inside(c.position),"Before-near point accepted");
            check(!inside(c.position+c.direction*24010.f),"Beyond-far point accepted");
        }
        Vec o,d;check(!c.ray(0,0,o,d),"Out of viewport click accepted");
        float x,y2;check(!c.project(c.position-c.direction*100,x,y2),"Behind-camera point accepted");
    }
    CameraSample c;c.valid=true;c.direction={0,1,0};c.up={0,0,1};c.right={1,0,0};c.frustum={-100,100,100,-100,1,1000,true,{}};c.width=c.height=200;
    Vec o,d;c.ray(150,50,o,d);check(o.x==50&&o.z==50&&d.y==1,"Screen axes must be right and up");
    Vec start{10,20,1000},dir=normalized(Vec{.3f,.2f,-1});
    Vec clipped=rayBelowHeight(start,dir,100);
    check(std::abs(clipped.z-100)<.001f,"Roof clipping failed to move below ceiling");
    check(length(cross(clipped-start,dir))<.001f,"Roof clipping changed the screen ray");
    check(rayBelowHeight(Vec{0,0,50},dir,100).z==50,"Already below roof ray moved");
    check(rayBelowHeight(start,Vec{1,0,0},100).z==1000,"Horizontal ray produced invalid clipping");

    // Expected screen locations across different render and output sizes, not
    // just an inverse-pair check using the same erroneous viewport twice.
    for(auto output: {ScreenPoint{1024,576},ScreenPoint{1920,1080},ScreenPoint{2560,1440},ScreenPoint{3440,1440},ScreenPoint{3840,2160}}){
        for(float renderScale:{.5f,1.f,2.f}){
            CameraSample world=c;world.x=output.x*renderScale*.1f;world.y=output.y*renderScale*.1f;world.width=output.x*renderScale*.8f;world.height=output.y*renderScale*.8f;
            auto screen=presentationCamera(world,output.x*renderScale,output.y*renderScale,output.x,output.y);
            float px{},py{};check(screen.project({50,100,50},px,py),"Quarter-position target invisible");
            check(std::abs(px-output.x*.7f)<.01f&&std::abs(py-output.y*.3f)<.01f,"Scaled world target not at expected final pixel");
            Vec origin,direction;check(screen.ray(output.x*.7f,output.y*.3f,origin,direction),"Scaled Alt pick rejected");
            check(std::abs(origin.x-50)<.01f&&std::abs(origin.z-50)<.01f,"Alt target changes with output resolution");
            check(!screen.ray(output.x*.05f,output.y*.5f,origin,direction),"Letterbox margin accepted");
        }
        UITransform ui{output.x,output.y,1706.6666f,960.f};auto label=ui.toUI({output.x*.7f,output.y*.3f});
        auto clickable=ui.toPixels(label);check(std::abs(clickable.x-output.x*.7f)<.01f&&std::abs(clickable.y-output.y*.3f)<.01f,"Prompt and hitbox disagree");
        auto extent=ui.toPixels({300,30});check(std::abs(extent.x/output.x-300/1706.6666f)<.0001f,"Prompt hitbox uses fixed pixels");
        auto resized=resizeCursor({960,270},1920,1080,output.x,output.y);check(std::abs(resized.x-output.x*.5f)<.01f&&std::abs(resized.y-output.y*.25f)<.01f,"Resolution change shifts cursor target");
    }
    for(auto output:{ScreenPoint{1024,768},ScreenPoint{1280,720},ScreenPoint{1920,1080},ScreenPoint{2560,1440},ScreenPoint{3440,1440},ScreenPoint{3840,2160}}){
        for(float menuHeight:{720.f,960.f,1200.f}){
            auto extent=nativeMenuExtent(output.x,output.y,menuHeight/output.y);
            check(std::abs(extent.y-menuHeight)<.001f,"Native menu converter ignored");
            UITransform ui{output.x,output.y,extent.x,extent.y};
            auto label=ui.damagePosition({output.x*.5f,output.y*.5f},160,1.f);
            auto centre=ui.toPixels({label.x+80,label.y});
            check(std::abs(centre.x-output.x*.5f)<.01f,"Damage label no longer centred");
            check(std::abs(centre.y-(output.y*.5f-(24.f+128.f/3.f)*output.y/menuHeight))<.01f,"Damage rise scales differently from text");
            check(std::abs(ui.hudPixelScale()*720.f-output.y*960.f/menuHeight)<.01f,"Overlay ignores native HUD scale");
            auto box=ui.toPixels({300,30});
            check(std::abs(box.x-300*output.y/menuHeight)<.01f,"Ultrawide stretches prompt width");
        }
    }
    check(nativeMenuExtent(2560,1440,0).x==0,"Uninitialized converter treated as pixel UI");
    check(nativeMenuExtent(2560,1440,NAN).x==0,"Invalid converter accepted");
    check(!presentationCamera(c,0,720,2560,1440).valid,"Unknown render surface accepted");
    std::cout<<"Resolution, intermediate surface, letterbox, UI hitbox and resize checks passed.\n";
    std::cout<<tests<<" camera round trips passed across pitch, yaw, projection and viewport offsets.\n";
}
