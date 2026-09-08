// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#include "../src/navmesh_path.hpp"
#include <iostream>
#include <cstdlib>
#include <chrono>
using namespace navigation;
void check(bool v,const char* s){if(!v){std::cerr<<s<<"\n";std::exit(1);}}
int main(){
 MeshTriangle a{{{0,0,0},{200,0,0},{0,200,0}},1,0,{-1,1,-1}};
 MeshTriangle b{{{200,0,0},{200,200,0},{0,200,0}},1,1,{-1,-1,0}};
 MeshGraph graph;std::vector<engine::Vec> path;
 graph.build({a,b},{});check(graph.find({10,10,0},{190,190,0},10,path),"Connected native triangles did not route");
 check(graph.expanded==0&&path.size()>2,"Triangle corridor not expanded/densified");
 auto routeLength=[](const std::vector<Vec>& points){float total=0;for(size_t i=1;i<points.size();i++)total+=engine::length(points[i]-points[i-1]);return total;};
 check(graph.find({10,150,0},{190,160,0},10,path),"Off-centre straight route failed");
 check(std::abs(routeLength(path)-engine::length(Vec{180,10,0}))<.1f,"Clear route detours via portal midpoint");
 b.side[2]=-1;graph.build({a,b},{});check(!graph.find({10,10,0},{190,190,0},10,path),"Disconnected touching triangles linked");
 b.mesh=2;b.index=0;graph.build({a,b},{});check(!graph.find({10,10,0},{190,190,0},10,path),"Unlinked mesh seam crossed");
 graph.build({a,b},{{1,2}});check(graph.find({10,10,0},{190,190,0},10,path),"Explicit loaded-mesh seam did not connect");
 check(!graph.find({10,10,150},{190,190,0},10,path),"Start projected through another floor");
 check(!graph.find({10,10,0},{1000,1000,0},10,path),"Off-mesh target accepted");
 graph.build({a},{});check(!graph.find({10,10,0},{190,190,0},10,path),"Missing/disabled triangle crossed");
 // Small triangles in an open surface must not be mistaken for a narrow doorway.
 auto smallA=a,smallB=b;smallB.mesh=1;smallB.index=1;smallB.side[2]=0;
 for(auto& v:smallA.v)v=v*.1f;for(auto& v:smallB.v)v=v*.1f;
 graph.build({smallA,smallB},{});check(graph.find({1,1,0},{19,19,0},2,path),"Short tessellation edge incorrectly blocked");
 // A large connected surface with a long missing strip forces a real detour.
 std::vector<MeshTriangle> field;constexpr int n=80;constexpr float cell=80;
 auto id=[](int x,int y){return unsigned((y*n+x)*2);};
 for(int y=0;y<n;y++)for(int x=0;x<n;x++){
  if(x==40&&y<70)continue;
  float l=x*cell,r=l+cell,d=y*cell,u=d+cell;auto k=id(x,y);
  field.push_back({{{l,d,0},{r,d,0},{l,u,0}},1,k,{y?int(id(x,y-1)+1):-1,int(k+1),x?int(id(x-1,y)+1):-1}});
  field.push_back({{{r,d,0},{r,u,0},{l,u,0}},1,k+1,{x<n-1?int(id(x+1,y)):-1,y<n-1?int(id(x,y+1)):-1,int(k)}});
 }
 auto started=std::chrono::steady_clock::now();graph.build(field,{});
 check(graph.find({20,20,0},{6380,20,0},10,path),"Large obstacle detour failed");
 auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
 bool detoured=false;
 for(auto p:path){detoured|=p.y>=5600;check(!(p.x>3200&&p.x<3280&&p.y<5600),"Route crossed missing obstacle strip");}
 check(detoured&&graph.expanded>100,"Complex test did not exercise a substantial search");
 std::cout<<"Large detour: "<<graph.size()<<" triangles, "<<graph.expanded<<" expanded, "<<ms<<" ms including graph build (synthetic).\n";
 // Open the near end of the strip as well: both ways are connected, but the
 // short bottom detour should win over walking around the far end.
 for(int y=0;y<10;y++){
  int x=40;float l=x*cell,r=l+cell,d=y*cell,u=d+cell;auto k=id(x,y);
  field.push_back({{{l,d,0},{r,d,0},{l,u,0}},1,k,{y?int(id(x,y-1)+1):-1,int(k+1),int(id(x-1,y)+1)}});
  field.push_back({{{r,d,0},{r,u,0},{l,u,0}},1,k+1,{int(id(x+1,y)),int(id(x,y+1)),int(k)}});
 }
 graph.build(field,{});check(graph.find({3000,1000,0},{3500,1000,0},10,path),"Two-way obstacle route failed");
 check(routeLength(path)<1200,"Search chose far end of obstacle instead of nearby opening");
 for(auto p:path)check(!(p.x>3200&&p.x<3280&&p.y>800&&p.y<5600),"Shortcut crossed obstacle");
 std::cout<<"Native triangle adjacency, seams, disconnected regions, floors and endpoints passed.\n";
}
