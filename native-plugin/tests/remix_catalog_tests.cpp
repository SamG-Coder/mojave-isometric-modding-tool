// SPDX-License-Identifier: GPL-3.0-only
// Copyright 2026 SamGCoder
#include "../src/remix_catalog.hpp"
#include <chrono>
#include <iostream>
#include <cstdlib>
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<"\n";std::exit(1);}}
int main(){using namespace remix_catalog;
 check(quote("a\"b\\c\n")=="\"a\\\"b\\\\c\\u000a\"","JSON escaping incorrect");
 check(sceneKey(1,10,10,45)==sceneKey(1,100,100,50),"Tiny moves create duplicate scenes");
 check(sceneKey(1,10,10,45)!=sceneKey(2,10,10,45),"Cells share scene key");
 auto file=std::filesystem::current_path()/"build/test-generated"/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())/"catalog.jsonl";
 Catalog first;check(first.open(file),"Catalogue open failed");check(first.add("ref:1:2","\"base\":123"),"Reference not added");check(!first.add("ref:1:2","\"base\":123"),"Repeated reference duplicated");check(first.flush(),"Catalogue flush failed");
 Catalog second;check(second.open(file)&&second.contains("ref:1:2"),"Catalogue did not survive restart");check(second.add("requested:scene","\"kind\":\"capture_request\""),"Capture request not saved");check(!second.contains("captured-scene:scene"),"Request falsely confirms capture");check(second.flush(),"Second flush failed");
 std::cout<<"Catalogue persistence, deduplication, scene keys, escaping and request separation passed.\n";
}
