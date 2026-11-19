// SPDX-License-Identifier: GPL-3.0-only
#include "movie.h"

#include <algorithm>
#include <cstdio>

namespace port {

void Movie::BeginRecording() {
    changes.clear();
    lastConfirmedFrame = 0;
}

bool Movie::Load(const std::string &path) {
    FILE *f = fopen(path.c_str(), "r");
    if (!f) {
        return false;
    }
    BeginRecording();
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        unsigned long long frame;
        unsigned p[4] = {};
        if (line[0] != '#' && sscanf(line, "%llu %x %x %x %x", &frame, &p[0], &p[1], &p[2], &p[3]) >= 3) {
            changes[frame] = {static_cast<uint16_t>(p[0]), static_cast<uint16_t>(p[1]), static_cast<uint16_t>(p[2]), static_cast<uint16_t>(p[3])};
        }
    }
    fclose(f);
    return true;
}

void Movie::RecordConfirmed(uint64_t frame, const Pads &pads) {
    // Confirmations normally arrive in order. A repeated frame replaces the previous sample.
    if (changes.empty() || frame <= changes.rbegin()->first || changes.rbegin()->second != pads) {
        changes.insert_or_assign(changes.end(), frame, pads);
    }
    lastConfirmedFrame = std::max(lastConfirmedFrame, frame);
}

bool Movie::Save(const std::string &path) const {
    FILE *f = fopen(path.c_str(), "w");
    if (!f) {
        return false;
    }
    // (as many players as any line has a press for, and two at least: a two-player movie is as it always was)
    int players = 2;
    for (const auto &[frame, pads] : changes) {
        for (int n = 2; n < static_cast<int>(pads.size()); n++) {
            players = pads[n] ? std::max(players, n + 1) : players;
        }
    }
    fprintf(f, "# cps2recomp input movie: frame player1 player2%s%s (pressed bits, hex)\n", players > 2 ? " player3" : "", players > 3 ? " player4" : "");
    const auto writePads = [&](uint64_t frame, const Pads &pads) {
        fprintf(f, "%llu", static_cast<unsigned long long>(frame));
        for (int n = 0; n < players; n++) {
            fprintf(f, " %04x", pads[n]);
        }
        fprintf(f, "\n");
    };
    for (const auto &[frame, pads] : changes) writePads(frame, pads);
    // A held input spans frames. Keep the end of an online recording visible to movie tools.
    if (!changes.empty() && lastConfirmedFrame > changes.rbegin()->first)
        writePads(lastConfirmedFrame, changes.rbegin()->second);
    fclose(f);
    return true;
}

} // namespace port
