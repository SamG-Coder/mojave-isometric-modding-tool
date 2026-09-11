// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamG-Coder
#include "../src/seated_visibility.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
using seated_visibility::field;
struct Memory { alignas(16) std::array<unsigned char,256> bytes{}; void* data(){return bytes.data();} };
void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
    Memory root,mesh,hidden,shader,hiddenShader,material;
    std::array<std::uintptr_t,4> nodeTable{0,0,0,0x6815C0};
    field<void*>(root.data(),0)=nodeTable.data();
    field<std::uint32_t>(root.data(),0x30)=0x4001;
    void* children[]={mesh.data(),hidden.data(),nullptr};
    field<void*>(root.data(),0xA0)=children;
    field<std::uint16_t>(root.data(),0xA6)=3;
    for(auto item:{mesh.data(),hidden.data()})field<std::uintptr_t>(item,0)=0x109D454;
    field<void*>(mesh.data(),0xA8)=shader.data();
    field<void*>(mesh.data(),0xA4)=material.data();
    field<float>(material.data(),0x3C)=.75f;
    field<float>(shader.data(),0x28)=.124f;
    field<float>(shader.data(),0x2C)=0.f;
    field<std::uint32_t>(shader.data(),0x38)=123;
    field<std::uint32_t>(hidden.data(),0x30)=1;
    field<void*>(hidden.data(),0xA8)=hiddenShader.data();
    field<float>(hiddenShader.data(),0x28)=.2f;
    {
        seated_visibility::WorldBodyScope scope(root.data());
        check(scope.repaired==1,"Expected only visible body geometry to be repaired");
        check((field<std::uint32_t>(root.data(),0x30)&0x8001)==0x8000,"Root not visible during traversal");
        check(field<float>(shader.data(),0x28)==.75f,"Authored material transparency lost");
        check(field<float>(shader.data(),0x2C)==1.f,"Camera fade still hides seated geometry");
        check(field<std::uint32_t>(shader.data(),0x38)==0,"Stale invisible render pass survived repair");
        check(field<float>(hiddenShader.data(),0x28)==.2f,"Hidden equipment variant changed");
        // Native traversal can set other flags; restoring visibility must not
        // discard those changes or leak IgnoreFade into the shadow pass.
        field<std::uint32_t>(root.data(),0x30)|=0x100000;
    }
    check(field<std::uint32_t>(root.data(),0x30)==0x104001,"Scope corrupted native visibility flags");
    check(field<float>(shader.data(),0x28)==.75f,"Opacity reverted before queued geometry was drawn");
    field<std::uint32_t>(shader.data(),0x38)=456;
    {seated_visibility::WorldBodyScope scope(root.data());check(scope.repaired==0,"Unchanged body invalidates render cache every pass");}
    check(field<std::uint32_t>(shader.data(),0x38)==456,"Stable render cache was discarded");
    {seated_visibility::WorldBodyScope scope(nullptr);check(scope.repaired==0,"Inactive scope changed surfaces");}
    std::cout<<"Seated body visibility tests passed\n";
}
