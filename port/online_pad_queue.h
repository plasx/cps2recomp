// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
#pragma once

#include "netplay.h"

namespace port {

// Local SDL polls and accepted online frames can run at different rates. Each entry is a whole poll of both
// local seats. A frame which actually runs consumes one entry. Rollback only sees the usual fixed input history.
class OnlinePadQueue {
public:
    using Pads = std::array<uint16_t, Netplay::kMostSeats>;
    static constexpr size_t kCapacity = 4;
    static constexpr uint64_t kMaxAgeMs = 100;

    const Pads &Poll(Pads held, uint64_t now) {
        latest = held;
        const bool changed = held != (count ? queued[count - 1].held : applied);
        if ((count && now - queued[0].when >= kMaxAgeMs) || (changed && count == kCapacity)) {
            // A long interruption or dense backlog should resume at what is held now.
            count = 0;
            liveUntilAccepted = true;
        }
        if (changed && !liveUntilAccepted) queued[count++] = {held, now};
        return count ? queued[0].held : latest;
    }

    void Accepted() {
        applied = count ? queued[0].held : latest;
        if (count) {
            for (size_t i = 1; i < count; ++i) queued[i - 1] = queued[i];
            --count;
        }
        liveUntilAccepted = false;
    }

    void Clear() {
        count = 0;
        latest = applied = {};
        liveUntilAccepted = false;
    }

    // Forget only this disconnected pad. Keep any transitions belonging to its local teammate.
    void ReleaseSeat(int seat) {
        if (seat < 0 || seat >= static_cast<int>(latest.size())) return;
        latest[seat] = applied[seat] = 0;
        Pads last = applied;
        size_t keep = 0;
        for (size_t i = 0; i < count; ++i) {
            auto sample = queued[i];
            sample.held[seat] = 0;
            if (sample.held != last) {
                queued[keep++] = sample;
                last = sample.held;
            }
        }
        count = keep;
    }

private:
    struct Sample { Pads held{}; uint64_t when = 0; };
    std::array<Sample, kCapacity> queued{};
    size_t count = 0;
    Pads latest{}, applied{};
    bool liveUntilAccepted = false;
};
} // namespace port
