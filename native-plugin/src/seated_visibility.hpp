// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamG-Coder
#pragma once
#include <cstdint>
#include <cmath>

namespace seated_visibility {
template<class T> T& field(void* p, unsigned offset) {
    return *reinterpret_cast<T*>(static_cast<char*>(p)+offset);
}

// Runtime 1.4.0.525 NiAVObject / BSShaderProperty layout. Run immediately
// before WORLD visibility traversal, not in the gameplay update: furniture
// camera updates can change fade/visibility between those two phases.
inline unsigned restoreSurfaces(void* node, unsigned depth=0) {
    if(!node || depth>32) return 0;
    const auto table=field<std::uintptr_t*>(node,0);
    const auto type=reinterpret_cast<std::uintptr_t>(table);
    unsigned repaired=0;
    if(type==0x109D454 || type==0x109CD44) {
        // Preserve hidden equipment variants, dismemberment caps and authored
        // material transparency. Never unhide individual geometry.
        if(field<std::uint32_t>(node,0x30)&1u) return 0;
        auto shader=field<void*>(node,0xA8);
        auto material=field<void*>(node,0xA4);
        const float alpha=material?field<float>(material,0x3C):1.f;
        if(shader && std::isfinite(alpha) && alpha>=0.f && alpha<=1.f) {
            if(field<float>(shader,0x28)!=alpha || field<float>(shader,0x2C)!=1.f) {
                field<float>(shader,0x28)=alpha;
                field<float>(shader,0x2C)=1.f;
                // Native SetAlpha (BA8AB0) invalidates this cache as well.
                // Updating the float alone can retain the invisible pass.
                field<std::uint32_t>(shader,0x38)=0;
                ++repaired;
            }
        }
    } else if(table && table[3]==0x6815C0) {
        if(depth && (field<std::uint32_t>(node,0x30)&1u)) return 0;
        auto children=field<void**>(node,0xA0);
        auto count=field<std::uint16_t>(node,0xA6);
        if(children && count<=512)
            for(unsigned i=0;i<count;++i) repaired+=restoreSurfaces(children[i],depth+1);
    }
    return repaired;
}

class WorldBodyScope {
    void* root_{};
    std::uint32_t flags_{};
public:
    unsigned repaired{};
    explicit WorldBodyScope(void* root):root_(root) {
        if(!root_) return;
        flags_=field<std::uint32_t>(root_,0x30);
        // Ignore camera-distance fading only for this player's world pass.
        // Other objects, shadow cameras and native menus retain their state.
        field<std::uint32_t>(root_,0x30)=(flags_&~1u)|0x8000u;
        repaired=restoreSurfaces(root_);
    }
    ~WorldBodyScope() {
        if(root_) {
            auto& flags=field<std::uint32_t>(root_,0x30);
            flags=(flags&~0x8001u)|(flags_&0x8001u);
        }
        // Surface state must survive traversal: the accumulated geometry is
        // drawn afterwards. Normal native updates resume when no longer seated.
    }
    WorldBodyScope(const WorldBodyScope&)=delete;
    WorldBodyScope& operator=(const WorldBodyScope&)=delete;
};
}
