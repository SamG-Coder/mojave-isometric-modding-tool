// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include "engine.hpp"
#include "navmesh_path.hpp"
#include <unordered_set>
namespace native_navigation {
// Runtime layouts verified against JIP GameForms/GameAPI and JohnnyGuitar GameAPI.
struct Triangle {int16_t vertex[3],side[3];uint32_t flags;};static_assert(sizeof(Triangle)==16);
inline bool plan(engine::Vec start,engine::Vec goal,float tolerance,std::vector<engine::Vec>& path,size_t& count,unsigned& expanded){
 using namespace engine;count=0;expanded=0;auto cell=at<void*>(player(),0x40);if(!cell)return false;
 auto array=at<void*>(cell,0x64);if(!array)return false;auto data=at<void**>(array,4);auto size=at<uint32_t>(array,8);if(!data||size>4096)return false;
 bool interior=(at<uint8_t>(cell,0x24)&1)!=0;auto world=at<void*>(cell,0xC0);
 std::vector<void*> meshes;std::unordered_set<void*> visited;for(unsigned i=0;i<size;i++)if(data[i])meshes.push_back(data[i]);
 std::vector<navigation::MeshTriangle> triangles;std::set<std::pair<uint32_t,uint32_t>> links;
 for(size_t m=0;m<meshes.size()&&visited.size()<128;m++){
  auto mesh=meshes[m];if(!visited.insert(mesh).second||at<uint8_t>(mesh,4)!=0x43)continue;
  auto parent=at<void*>(mesh,0x24);if(!parent||(interior?parent!=cell:at<void*>(parent,0xC0)!=world))continue;
  // Only resident navigation belonging to a loaded cell; never load new cells.
  if(at<uint8_t>(parent,0x26)!=6&&parent!=cell)continue;
  auto info=at<void*>(mesh,0x104);if(info&&(at<uint32_t>(info,8)&0x10))continue;
  auto vertices=at<Vec*>(mesh,0x2C);auto nv=at<uint32_t>(mesh,0x30);auto ts=at<Triangle*>(mesh,0x3C);auto nt=at<uint32_t>(mesh,0x40);
  if(!vertices||!ts||nv>65535||nt>65535||triangles.size()+nt>100000)continue;
  uint32_t id=at<uint32_t>(mesh,0xC);
  for(unsigned i=0;i<nt;i++){auto t=ts[i];if(t.flags&0x20)continue;bool valid=true;navigation::MeshTriangle copy{};copy.mesh=id;copy.index=i;
   for(int j=0;j<3;j++){auto vi=uint16_t(t.vertex[j]);if(vi>=nv){valid=false;break;}copy.v[j]=vertices[vi];copy.side[j]=t.side[j];auto v=copy.v[j];if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)){valid=false;break;}}
   if(valid)triangles.push_back(copy);
  }
  auto extras=at<char*>(mesh,0x4C);auto ne=at<uint32_t>(mesh,0x50);if(extras&&ne<65536)for(unsigned e=0;e<ne;e++){
   auto nextInfo=at<void*>(extras,e*12+4);if(!nextInfo||(at<uint32_t>(nextInfo,8)&0x10))continue;
   auto nextId=at<uint32_t>(nextInfo,0);auto next=reference(nextId);if(next&&at<uint8_t>(next,4)==0x43){links.insert({id,nextId});if(!visited.count(next)&&meshes.size()<4096)meshes.push_back(next);}
  }
 }
 navigation::MeshGraph graph;graph.build(triangles,links);count=graph.size();bool found=graph.find(start,goal,tolerance,path);expanded=graph.expanded;return found;
}
}
