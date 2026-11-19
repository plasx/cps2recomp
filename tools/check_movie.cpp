// SPDX-License-Identifier: GPL-3.0-only
#include "../port/movie.h"

#include <cassert>
#include <cstdio>

int main(int argc, char **argv) {
    assert(argc == 2);
    port::Movie record;
    const port::Movie::Pads first{1, 2, 3, 4}, second{4, 3, 2, 1};
    record.RecordConfirmed(0, first);
    record.RecordConfirmed(4000, second);
    record.BeginRecording(); // A rehost or a spectator starting at a confirmed snapshot.
    record.RecordConfirmed(120, first);
    record.RecordConfirmed(121, first);
    record.RecordConfirmed(121, second);
    record.RecordConfirmed(121, first);
    record.RecordConfirmed(125, first);
    record.RecordConfirmed(120, second);
    assert(record.changes.size() == 2 && record.lastConfirmedFrame == 125);
    assert(record.changes.at(120) == second && record.changes.at(121) == first);
    assert(record.Save(argv[1]));

    port::Movie replay;
    assert(replay.Load(argv[1]));
    assert(replay.changes.size() == 3 && replay.changes.at(125) == first);
    assert(replay.changes.at(120) == second && replay.changes.at(121) == first);
    record.BeginRecording();
    record.RecordConfirmed(0, first);
    record.RecordConfirmed(1, second);
    record.RecordConfirmed(0, second);
    assert(record.changes.size() == 2 && record.changes.at(1) == second);
    assert(record.Save(argv[1]) && replay.Load(argv[1]));
    assert(replay.changes == record.changes);
    std::remove(argv[1]);
    std::puts("confirmed movies: rehost, nonzero first frame, replacement and four-pad save/load passed");
}
