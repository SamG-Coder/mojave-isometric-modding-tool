// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamGCoder
#pragma once
#include <atomic>
#include <algorithm>
class WheelInput {
    std::atomic<int> pending{0};
public:
    void filterMotion(long& x,long& y,long& wheel,bool ownsCursor,bool ownsLook,
                      std::atomic<int>& cursorX,std::atomic<int>& cursorY){
        filter(wheel,ownsLook);
        if(ownsCursor){cursorX.fetch_add(int(x));cursorY.fetch_add(int(y));x=y=0;}
        else {cursorX=0;cursorY=0;if(ownsLook)x=y=0;}
    }
    void filter(long& delta,bool ownsWheel){
        if(!ownsWheel){pending.store(0);return;}
        pending.fetch_add(int(std::clamp(delta,-12000L,12000L)));delta=0;
    }
    int take(){return std::clamp(pending.exchange(0),-12000,12000);}
    void reset(){pending.store(0);}
};
