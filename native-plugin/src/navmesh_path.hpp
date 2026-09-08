// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include "camera_math.hpp"
#include <vector>
#include <array>
#include <unordered_map>
#include <set>
#include <queue>
#include <algorithm>
#include <limits>
namespace navigation {
using engine::Vec;
struct MeshEdgeTarget {uint32_t mesh{};int triangle{-1};};
struct MeshTriangle {Vec v[3];uint32_t mesh{},index{};int side[3];MeshEdgeTarget external[3]{};};
class MeshGraph {
 struct Link{int triangle,edge,otherEdge;};
 struct Triangle{MeshTriangle source;Vec centre;std::vector<Link> links;};
 std::vector<Triangle> triangles;
 static Vec nearest(Vec p,const MeshTriangle& t){
  auto a=t.v[0],b=t.v[1],c=t.v[2];float denom=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
  if(std::abs(denom)>1e-5f){float u=((b.y-c.y)*(p.x-c.x)+(c.x-b.x)*(p.y-c.y))/denom;
   float v=((c.y-a.y)*(p.x-c.x)+(a.x-c.x)*(p.y-c.y))/denom;
   if(u>=0&&v>=0&&u+v<=1)return a*u+b*v+c*(1-u-v);
  }
  Vec best=a;float d=engine::length(p-a);
  for(int i=0;i<3;i++){auto start=t.v[i],delta=t.v[(i+1)%3]-start;float len=engine::dot(delta,delta);if(len<.001f)continue;
   auto q=start+delta*std::clamp(engine::dot(p-start,delta)/len,0.f,1.f);float next=engine::length(q-p);if(next<d){d=next;best=q;}}
  return best;
 }
 int locate(Vec point,float tolerance,Vec& projected)const{
  int result=-1;float best=tolerance;
  for(size_t i=0;i<triangles.size();i++){auto q=nearest(point,triangles[i].source);float d=engine::length(q-point);if(d<best){best=d;projected=q;result=int(i);}}
  return result;
 }
 // Clip a segment to the triangle's XY footprint. Height checks retain floor
 // identity and prevent smoothing across large changes in slope.
 bool interval(int id,Vec a,Vec b,float& lo,float& hi)const{
  const auto& t=triangles[id].source;lo=0;hi=1;
  auto cross=[](Vec x,Vec y){return x.x*y.y-x.y*y.x;};
  float area=cross(t.v[1]-t.v[0],t.v[2]-t.v[0]);if(std::abs(area)<1e-5f)return false;
  float sign=area>0?1.f:-1.f;
  for(int e=0;e<3;e++){
   auto edge=t.v[(e+1)%3]-t.v[e];float d=sign*cross(edge,a-t.v[e]),v=sign*cross(edge,b-a);
   if(std::abs(v)<1e-6f){if(d<-.01f)return false;continue;}
   float q=-d/v;if(v>0)lo=std::max(lo,q);else hi=std::min(hi,q);
  }
  if(lo>hi+1e-6f)return false;
  for(float f:{lo,hi}){auto point=a+(b-a)*f;if(engine::length(nearest(point,t)-point)>40)return false;}
  return true;
 }
 bool straight(int first,int last,Vec a,Vec b,std::vector<Vec>& out)const{
  struct Visit{int id,parent;float exit;};
  float lo,hi;if(!interval(first,a,b,lo,hi)||lo>1e-5f)return false;
  std::vector<Visit> queue{{first,-1,hi}};std::vector<bool> seen(triangles.size());seen[first]=true;
  for(size_t i=0;i<queue.size();i++){
   auto visit=queue[i];
   if(visit.id==last&&visit.exit>=1-1e-5f){
    std::vector<Vec> reverse{b};
    for(int j=int(i);queue[j].parent>=0;j=queue[j].parent){auto prev=queue[queue[j].parent];auto p=a+(b-a)*prev.exit;reverse.push_back(nearest(p,triangles[prev.id].source));}
    out.assign(reverse.rbegin(),reverse.rend());return true;
   }
   for(auto link:triangles[visit.id].links){
    if(seen[link.triangle]||!interval(link.triangle,a,b,lo,hi)||lo>visit.exit+1e-5f||hi<visit.exit-1e-5f)continue;
    seen[link.triangle]=true;queue.push_back({link.triangle,int(i),hi});
   }
  }
  return false;
 }
public:
 unsigned expanded{};size_t size()const{return triangles.size();}
 void build(const std::vector<MeshTriangle>& input){
  triangles.clear();triangles.reserve(input.size());std::unordered_map<uint64_t,int> ids;ids.reserve(input.size());
  auto key=[](uint32_t mesh,uint32_t triangle){return (uint64_t(mesh)<<32)|triangle;};
  for(auto source:input){ids.emplace(key(source.mesh,source.index),int(triangles.size()));triangles.push_back({source,(source.v[0]+source.v[1]+source.v[2])*(1.f/3),{}});}
  for(size_t i=0;i<triangles.size();i++){
   auto& source=triangles[i].source;
   for(int edge=0;edge<3;edge++){
    auto target=source.external[edge];bool external=target.mesh!=0;
    if(!external)target={source.mesh,source.side[edge]};
    if(target.triangle<0)continue;auto found=ids.find(key(target.mesh,uint32_t(target.triangle)));if(found==ids.end())continue;
    auto& other=triangles[found->second].source;
    for(int e=0;e<3;e++){
     auto back=other.external[e];bool reciprocal=external?(back.mesh==source.mesh&&back.triangle==int(source.index)):
       (!back.mesh&&other.mesh==source.mesh&&other.side[e]==int(source.index));
     if(reciprocal){triangles[i].links.push_back({found->second,edge,e});break;}
    }
   }
  }
 }
 bool find(Vec start,Vec goal,float goalTolerance,std::vector<Vec>& out){
  out.clear();expanded=0;Vec from{},to{};int first=locate(start,80,from),last=locate(goal,goalTolerance,to);if(first<0||last<0)return false;
  std::vector<Vec> corners;
  if(!straight(first,last,from,to,corners)){
   // Search entry portals, not triangle centres. Different entries into a large
   // triangle remain distinct states, with costs including the actual endpoints.
   struct Entry{float f;int id;bool operator<(const Entry& b)const{return f>b.f;}};
   const size_t count=triangles.size()*3;
   std::priority_queue<Entry> open;std::vector<float> cost(count,std::numeric_limits<float>::infinity());
   std::vector<int> parent(count,-1);std::vector<bool> closed(count);
   auto point=[&](int state){const auto& t=triangles[state/3].source;int edge=state%3;return (t.v[edge]+t.v[(edge+1)%3])*.5f;};
   auto expand=[&](int triangle,Vec pos,float g,int previous){
    for(auto link:triangles[triangle].links){int next=link.triangle*3+link.otherEdge;auto p=point(next);auto& t=triangles[triangle].source;auto exit=(t.v[link.edge]+t.v[(link.edge+1)%3])*.5f;float nextCost=g+engine::length(exit-pos)+engine::length(p-exit);
     if(nextCost<cost[next]){cost[next]=nextCost;parent[next]=previous;open.push({nextCost+engine::length(to-p),next});}}
   };
   expand(first,from,0,-1);int finish=-1;
   while(!open.empty()){int state=open.top().id;open.pop();if(closed[state])continue;closed[state]=true;++expanded;
    if(state/3==last){finish=state;break;}expand(state/3,point(state),cost[state],state);
   }
   if(finish<0)return false;
   std::vector<Vec> raw;std::vector<int> ids;
   for(int s=finish;s>=0;s=parent[s]){
    raw.push_back(point(s));ids.push_back(s/3);
    int previous=parent[s]>=0?parent[s]/3:first;
    for(auto link:triangles[previous].links)if(link.triangle==s/3&&link.otherEdge==s%3){
     const auto& t=triangles[previous].source;raw.push_back((t.v[link.edge]+t.v[(link.edge+1)%3])*.5f);ids.push_back(previous);break;
    }
   }
   std::reverse(raw.begin(),raw.end());std::reverse(ids.begin(),ids.end());raw.push_back(to);ids.push_back(last);
   // Shorten through connected triangles only. Try long shortcuts first, reducing
   // the lookahead on failure so obstacle detours do not trigger quadratic work.
   Vec current=from;int triangle=first;size_t next=0;
   while(next<raw.size()){
    size_t candidate=raw.size()-1;std::vector<Vec> section;
    while(candidate>next&&!straight(triangle,ids[candidate],current,raw[candidate],section))candidate=next+(candidate-next)/2;
    if(candidate==next){section.clear();if(!straight(triangle,ids[next],current,raw[next],section))section.push_back(raw[next]);}
    corners.insert(corners.end(),section.begin(),section.end());current=raw[candidate];triangle=ids[candidate];next=candidate+1;
   }
  }
  out.push_back(start);
  // Preserve surface crossings in Z and keep follower segments locally verifiable.
  for(auto end:corners){auto begin=out.back();auto delta=end-begin;int steps=std::max(1,int(std::ceil(engine::length(delta)/64)));
   for(int i=1;i<=steps;i++)out.push_back(begin+delta*(float(i)/steps));}
  return true;
 }
};
}
