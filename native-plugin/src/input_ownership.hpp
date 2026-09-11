// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 SamG-Coder
#pragma once
#include <string>
namespace input_ownership {
// xNVSE: bit 0 suppresses physical input; bit 1 suppresses script input.
// Our fire/aim commands must survive while physical clicks belong to the UI.
inline std::string command(bool acquire,int control) {
    return std::string(acquire?"DisableControl ":"EnableControl ")+std::to_string(control)+" 1";
}
}
