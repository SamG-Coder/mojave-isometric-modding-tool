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
    std::cout<<tests<<" camera round trips passed across pitch, yaw, projection and viewport offsets.\n";
}
