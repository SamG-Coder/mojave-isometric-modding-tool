// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include <atomic>
#include <algorithm>
class WheelInput {
    std::atomic<int> pending{0};
public:
    void filter(long& delta,bool ownsWheel){
        if(!ownsWheel){pending.store(0);return;}
        pending.fetch_add(int(std::clamp(delta,-12000L,12000L)));delta=0;
    }
    int take(){return std::clamp(pending.exchange(0),-12000,12000);}
    void reset(){pending.store(0);}
};
