// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include "camera_math.hpp"
#include <vector>
#include <unordered_map>
#include <queue>
#include <functional>
#include <algorithm>
#include <limits>
#include <tuple>
#include <chrono>
namespace navigation {
using engine::Vec;
class Search {
    struct Node {Vec p{};float g=std::numeric_limits<float>::infinity();int parent=-1;bool closed=false;int x{},y{};};
    struct Entry{float score;int node;float remaining{};bool operator<(const Entry& other)const{return score==other.score?remaining>other.remaining:score>other.score;}};
    struct PointKey {float x,y,z;bool operator==(const PointKey&)const=default;};
    struct PointHash {size_t operator()(const PointKey& p)const{
        size_t h=std::hash<float>{}(p.x);h^=std::hash<float>{}(p.y)+0x9e3779b9+(h<<6)+(h>>2);return h^(std::hash<float>{}(p.z)+0x9e3779b9+(h<<6)+(h>>2));
    }};
    struct EdgeHash {size_t operator()(const std::pair<PointKey,PointKey>& p)const{size_t h=PointHash{}(p.first);return h^(PointHash{}(p.second)+0x9e3779b9+(h<<6)+(h>>2));}};
    struct GridHash {size_t operator()(const std::tuple<int,int,int>& p)const{size_t h=std::hash<int>{}(std::get<0>(p));h^=std::hash<int>{}(std::get<1>(p))+0x9e3779b9+(h<<6)+(h>>2);return h^(std::hash<int>{}(std::get<2>(p))+0x9e3779b9+(h<<6)+(h>>2));}};
    static PointKey pointKey(Vec p){return {p.x,p.y,p.z};}
    struct FloorSample{bool valid;Vec position;};
    std::unordered_map<PointKey,FloorSample,PointHash> floors;
    std::unordered_map<std::pair<PointKey,PointKey>,bool,EdgeHash> edges;
    std::unordered_map<std::tuple<int,int,int>,int,GridHash> ids;
    std::vector<Node> nodes;
    // Retain heap capacity between orders instead of reallocating it each time.
    struct OpenHeap {
        std::vector<Entry> entries;
        bool empty()const{return entries.empty();}
        const Entry& top()const{return entries.front();}
        void push(Entry e){entries.push_back(e);std::push_heap(entries.begin(),entries.end());}
        void pop(){std::pop_heap(entries.begin(),entries.end());entries.pop_back();}
        void clear(){entries.clear();}
    } open;
    Vec origin{},goal{},directPosition{};float reach{};bool testingDirect{};std::vector<Vec> directPath;
    float heuristic(Vec p)const{return std::max(0.f,std::hypot(goal.x-p.x,goal.y-p.y)-reach);}
    void finish(int id,bool exact){path.clear();for(int n=id;n>=0;n=nodes[n].parent)path.push_back(nodes[n].p);std::reverse(path.begin(),path.end());if(exact)path.push_back(goal);state=State::Found;}
public:
    Search(){nodes.reserve(2048);floors.reserve(4096);edges.reserve(8192);ids.reserve(2048);open.entries.reserve(2048);}
    enum class State {Idle,Searching,Found,NoPath};State state=State::Idle;
    std::vector<Vec> path;bool directRoute{};unsigned expanded{},groundQueries{},edgeQueries{},cacheHits{};
    static constexpr float spacing=32;
    // Sample a reachable floor and test a player-width connection. Callbacks run on the game thread.
    using Ground=std::function<bool(Vec,Vec&)>;using Clear=std::function<bool(Vec,Vec)>;
    void cancel(){state=State::Idle;testingDirect=false;directRoute=false;directPath.clear();path.clear();open.clear();nodes.clear();ids.clear();floors.clear();edges.clear();}
    void begin(Vec start,Vec end,float radius){cancel();origin=start;goal=end;reach=radius;directPosition=start;directPath={start};testingDirect=true;expanded=groundQueries=edgeQueries=cacheHits=0;Node n;n.p=start;n.g=0;nodes.push_back(n);ids[{0,0,int(std::floor(start.z/16))}]=0;open.push({heuristic(start),0});state=State::Searching;}
    template<class GroundQuery,class ClearanceQuery>
    void step(const GroundQuery& ground,const ClearanceQuery& clear,unsigned budget=6,float milliseconds=2.f){
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::duration<float,std::milli>(milliseconds);
        auto sample=[&](Vec guess,Vec& out){auto key=pointKey(guess);auto found=floors.find(key);
            if(found!=floors.end()){++cacheHits;out=found->second.position;return found->second.valid;}
            ++groundQueries;bool valid=ground(guess,out);floors[key]={valid,out};return valid;};
        auto connection=[&](Vec a,Vec b){auto key=std::make_pair(pointKey(a),pointKey(b));auto found=edges.find(key);
            if(found!=edges.end()){++cacheHits;return found->second;}
            ++edgeQueries;return edges[key]=clear(a,b);};

        // Validate a straight corridor first. Each segment checks floor support,
        // step height and body clearance; no speculative movement through walls.
        // Long corridors yield under the same frame budget as A*.
        while(state==State::Searching&&testingDirect&&budget){
            --budget;Vec delta=goal-directPosition;float horizontal=std::hypot(delta.x,delta.y);
            float tolerance=reach<=25?40.f:150.f;
            if(horizontal<=reach&&std::abs(delta.z)<tolerance){
                path=directPath;state=State::Found;directRoute=true;return;
            }
            if(horizontal<.01f){testingDirect=false;break;}
            float travel=std::min(spacing,horizontal);
            Vec guess=directPosition+Vec{delta.x,delta.y,0}*(travel/horizontal),floor{};
            if(!sample(guess,floor)||std::abs(floor.z-directPosition.z)>40||!connection(directPosition,floor)){
                testingDirect=false;directPath.clear();break;
            }
            directPosition=floor;directPath.push_back(floor);
            if(travel>=horizontal-.01f&&std::abs(goal.z-floor.z)>=tolerance){testingDirect=false;directPath.clear();break;}
            if(std::chrono::steady_clock::now()>=deadline)return;
        }
        while(state==State::Searching&&budget--){
            while(!open.empty()&&nodes[open.top().node].closed)open.pop();
            if(open.empty()||expanded>=6000){state=State::NoPath;return;}
            int id=open.top().node;open.pop();nodes[id].closed=true;Node current=nodes[id];++expanded;
            float d=std::hypot(goal.x-current.p.x,goal.y-current.p.y);
            if(d<=reach&&std::abs(goal.z-current.p.z)<(reach<=25?40.f:150.f)){finish(id,false);return;}
            if(reach<=25&&d<spacing*1.6f&&connection(current.p,goal)){finish(id,true);return;}
            for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++){
                if(!dx&&!dy)continue;int x=current.x+dx,y=current.y+dy;
                if(std::abs(x)>220||std::abs(y)>220)continue;
                Vec dest{};
                if(!sample({origin.x+x*spacing,origin.y+y*spacing,current.p.z},dest))continue;
                auto key=std::make_tuple(x,y,int(std::floor(dest.z/16)));int next;auto found=ids.find(key);
                if(found==ids.end()){
                    next=int(nodes.size());Node node;node.p=dest;node.x=x;node.y=y;nodes.push_back(node);ids[key]=next;
                }else {next=found->second;dest=nodes[next].p;}
                if(nodes[next].closed||std::abs(dest.z-current.p.z)>40.f)continue;
                if(dx&&dy){Vec a,b;if(!sample({dest.x,current.p.y,current.p.z},a)||!sample({current.p.x,dest.y,current.p.z},b)||!connection(current.p,a)||!connection(current.p,b)||!connection(a,dest)||!connection(b,dest))continue;}
                if(!connection(current.p,dest))continue;
                float cost=current.g+engine::length(dest-current.p);
                if(cost<nodes[next].g){nodes[next].g=cost;nodes[next].parent=id;open.push({cost+heuristic(dest),next,heuristic(dest)});}
            }
            // The budget yields between expansions; one expansion remains atomic.
            if(std::chrono::steady_clock::now()>=deadline)return;
        }
    }
};
}
