// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#pragma once
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <cmath>
#include <cstdio>
namespace remix_catalog {
inline std::string quote(const std::string& s){std::string r="\"";for(unsigned char c:s){if(c=='"'||c=='\\'){r+='\\';r+=char(c);}else if(c>=32)r+=char(c);else {char b[7];snprintf(b,sizeof(b),"\\u%04x",c);r+=b;}}return r+'"';}
inline std::string sceneKey(unsigned cell,float x,float y,float yaw){return std::to_string(cell)+":"+std::to_string(int(std::floor(x/768)))+":"+std::to_string(int(std::floor(y/768)))+":"+std::to_string(int(std::floor((std::remainder(yaw,360.f)+180)/90)));}
class Catalog {
 std::filesystem::path file;std::set<std::string> keys;std::string pending;
public:
 bool open(const std::filesystem::path& p){file=p;std::error_code ec;std::filesystem::create_directories(p.parent_path(),ec);if(ec)return false;std::ifstream in(p);std::string line;while(std::getline(in,line)){auto a=line.find("\"key\":\"");if(a==std::string::npos)continue;a+=7;auto b=line.find('"',a);if(b!=std::string::npos)keys.insert(line.substr(a,b-a));}return true;}
 bool contains(const std::string& key)const{return keys.count(key)!=0;}
 bool add(const std::string& key,const std::string& fields){if(contains(key))return false;pending+="{\"key\":"+quote(key)+","+fields+"}\n";keys.insert(key);return true;}
 bool flush(){if(pending.empty())return true;std::ofstream out(file,std::ios::app);if(!out)return false;out<<pending;out.flush();if(!out)return false;pending.clear();return true;}
 size_t size()const{return keys.size();}
};
}
