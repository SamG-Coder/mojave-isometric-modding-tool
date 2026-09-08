// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include "camera_math.hpp"
#include <vector>
#include <map>
#include <queue>
#include <functional>
#include <algorithm>
#include <limits>
namespace navigation {
using engine::Vec;
class Search {
    struct Node {Vec p{};float g=std::numeric_limits<float>::infinity();int parent=-1;bool closed=false;int x{},y{};};
    struct Entry{float score;int node;bool operator<(const Entry& other)const{return score>other.score;}};
    std::map<std::pair<int,int>,int> ids;std::vector<Node> nodes;std::priority_queue<Entry> open;
    Vec origin{},goal{};float reach{};
    float heuristic(Vec p)const{return std::max(0.f,std::hypot(goal.x-p.x,goal.y-p.y)-reach);}
    void finish(int id,bool exact){path.clear();for(int n=id;n>=0;n=nodes[n].parent)path.push_back(nodes[n].p);std::reverse(path.begin(),path.end());if(exact)path.push_back(goal);state=State::Found;}
public:
    enum class State {Idle,Searching,Found,NoPath};State state=State::Idle;
    std::vector<Vec> path;unsigned expanded{};
    static constexpr float spacing=64;
    // Sample a reachable floor and test a player-width connection. Callbacks run on the game thread.
    using Ground=std::function<bool(Vec,Vec&)>;using Clear=std::function<bool(Vec,Vec)>;
    void cancel(){state=State::Idle;path.clear();open={};nodes.clear();ids.clear();}
    void begin(Vec start,Vec end,float radius){cancel();origin=start;goal=end;reach=radius;expanded=0;Node n;n.p=start;n.g=0;nodes.push_back(n);ids[{0,0}]=0;open.push({heuristic(start),0});state=State::Searching;}
    void step(const Ground& ground,const Clear& clear,unsigned budget=6){
        while(state==State::Searching&&budget--){
            while(!open.empty()&&nodes[open.top().node].closed)open.pop();
            if(open.empty()||expanded>=6000){state=State::NoPath;return;}
            int id=open.top().node;open.pop();nodes[id].closed=true;Node current=nodes[id];++expanded;
            float d=std::hypot(goal.x-current.p.x,goal.y-current.p.y);
            if(d<=reach&&std::abs(goal.z-current.p.z)<150){finish(id,false);return;}
            if(reach<=25&&d<spacing*1.6f&&clear(current.p,goal)){finish(id,true);return;}
            for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++){
                if(!dx&&!dy)continue;int x=current.x+dx,y=current.y+dy;
                if(std::abs(x)>110||std::abs(y)>110)continue;
                auto key=std::make_pair(x,y);int next;auto found=ids.find(key);Vec dest;
                if(found==ids.end()){
                    if(!ground({origin.x+x*spacing,origin.y+y*spacing,current.p.z},dest))continue;
                    next=int(nodes.size());Node node;node.p=dest;node.x=x;node.y=y;nodes.push_back(node);ids[key]=next;
                }else {next=found->second;dest=nodes[next].p;}
                if(nodes[next].closed||std::abs(dest.z-current.p.z)>spacing*.75f)continue;
                if(dx&&dy){Vec a,b;if(!ground({dest.x,current.p.y,current.p.z},a)||!ground({current.p.x,dest.y,current.p.z},b)||!clear(current.p,a)||!clear(current.p,b)||!clear(a,dest)||!clear(b,dest))continue;}
                if(!clear(current.p,dest))continue;
                float cost=current.g+engine::length(dest-current.p);
                if(cost<nodes[next].g){nodes[next].g=cost;nodes[next].parent=id;open.push({cost+heuristic(dest),next});}
            }
        }
    }
};
}
