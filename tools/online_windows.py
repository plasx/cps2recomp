#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
"""Print CSV deltas between consecutive online reports, grouped by source log and match.

Example: python3 tools/online_windows.py online-host.err online-join*.err
Frame counts are changes in reported frame number. Prediction and pacing waits are
counts of polling ticks. A blank field means that counter was absent from the log.
"""
import argparse
import csv
from pathlib import Path
import re
import sys

REPORT = re.compile(
    r"^online: frame (\d+), ping [\d.]+ ms, (\d+) rollbacks "
    r"\(\d+ since the last report\), (\d+) frames run again, at most \d+ at once; "
    r"\d+ ticks waited; a frame at most [\d.]+ ms, (\d+) over a frame's time"
)
WAITS = re.compile(r"^online: delay \d+ frames?, prediction waits (\d+), pacing waits (\d+)")
START = re.compile(r"^online: (?:the match is on:|\S+ \S+, protocol \d+, game signature )")
COUNTERS = ('rollbacks', 'resimulated_frames', 'frame_overruns', 'prediction_wait_ticks', 'pacing_wait_ticks')


def samples(lines):
    """None marks a match start. Hold a report until its optional waits line has arrived."""
    pending = None
    for line in lines:
        match = REPORT.match(line)
        if match or START.match(line):
            if pending is not None:
                yield pending
                pending = None
            if match:
                pending = tuple(map(int, match.groups())) + (None, None)
            else:
                yield None
        elif pending is not None:
            waits = WAITS.match(line)
            if waits:
                yield pending[:4] + tuple(map(int, waits.groups()))
                pending = None
    if pending is not None:
        yield pending


def intervals(lines):
    session = 0
    previous = None
    saw_start = False
    for current in samples(lines):
        if current is None:
            previous = None
            saw_start = True
            continue
        if previous is not None and any(
            a is not None and b is not None and a < b for a, b in zip(current, previous)
        ):
            # Two logs can be joined without also retaining the connection lines.
            previous = None
            saw_start = False
        if previous is None:
            session += 1
            if not saw_start:
                previous = current  # first sample from a partial log is its baseline
                continue
            previous = (0,) * len(current)
            saw_start = False
        delta = [a - b if a is not None and b is not None else None for a, b in zip(current, previous)]
        if any(delta):
            yield (session, previous[0], current[0], *delta)
        previous = current


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('logs', nargs='+', type=Path, help='one game log per playing machine')
    args = parser.parse_args()
    output = csv.writer(sys.stdout, lineterminator='\n')
    output.writerow(('file', 'session', 'from_frame', 'to_frame', 'frames_advanced', *COUNTERS))
    for path in args.logs:
        try:
            with path.open(encoding='utf-8', errors='replace') as stream:
                for row in intervals(stream):
                    output.writerow((str(path), *row))
        except OSError as error:
            parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
