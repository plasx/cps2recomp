// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "../machine/machine.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>

namespace port {

// Recorded input: what the players hold, at the frames where it changes
// (each line: the frame, then each player's; two players unless a movie has more)
struct Movie {
    using Pads = std::array<uint16_t, cps2recomp::Machine::kMaxPlayers>;
    std::map<uint64_t, Pads> changes;
    uint64_t lastConfirmedFrame = 0;
    void BeginRecording();
    void RecordConfirmed(uint64_t frame, const Pads &pads);
    bool Load(const std::string &path);
    bool Save(const std::string &path) const;
};

} // namespace port
