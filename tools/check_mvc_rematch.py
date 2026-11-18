#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morphus56K and contributors. Part of cps2recomp.
"""Play two MVC matches from boot and verify who can choose between them."""
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MOVIE = ROOT / 'games/mvsc/movies/cross_fever.txt'
# (room layout, first result, press the losing team's Start, enable Everyone reselects)
CASES = {
    'left-ko': (2, 'left', False, True),
    'right-ko': (2, 'right', False, True),
    'clock': (2, 'time', False, True),
    'draw': (2, 'draw', False, True),
    'early-start': (2, 'left', True, True),
    'arcade-continue': (2, 'left', True, False),
    'two-vs-one': (3, 'left', False, True),
    'one-vs-two': (4, 'left', False, True),
}


def make_movie(target, mode, result, start):
    rows = {}
    for line in MOVIE.read_text().splitlines():
        if line and not line.startswith('#'):
            f, *words = line.split()
            if int(f) < 4100:
                rows[int(f)] = [int(w, 16) for w in words]
    neutral = [0] * 4
    if result in ('left', 'time'):
        stop = 6202 if result == 'left' else 4530
        for f in range(4330, stop - 9, 20):
            rows[f] = [0x41, 0, 0, 0]
            rows[f + 9] = [1, 0, 0, 0]
        rows[stop] = neutral
    elif result == 'right':
        rows[4300] = [1, 2, 0, 0]  # walk together, then sweep the advancing opponent
        for f in range(4450, 7000, 28):
            rows[f] = [1, 0x204, 0, 0]
            rows[f + 12] = [1, 0, 0, 0]
        rows[7009] = neutral
    if start:
        rows[6290] = [0, 0x400, 0, 0]
        rows[6295] = neutral
    # Each owner confirms alone. The first player on the solo team confirms both that team's picks.
    select = {'left': 7100, 'right': 8000, 'time': 14500, 'draw': 14500}[result]
    for f in range(select, select + 2010, 136):
        for seat, command in enumerate((0x11, 0x12, 0x21, 0x22)):
            words = [0]*4
            words[seat] = command
            rows[f + 18 * seat] = words
            rows[f + 18 * seat + 5] = neutral
    owners = {}
    with target.open('w') as file:
        for frame, (a, b, c, d) in sorted(rows.items()):
            words = (a, b, c, d)
            if mode == 3:
                words = (a, b | d, c, 0)
            elif mode == 4:
                words = (a | c, b, d, 0)
            owners[frame] = words
            print(frame, *(f'{w:04x}' for w in words), file=file)
    return select + 2200, owners


def verify(trace, ram, frames, owners, mode, result, everyone):
    expected = {'left': 1, 'right': 2, 'time': 1, 'draw': 255}[result]
    order = {2: ((0, 2), (1, 3)), 3: ((0, 2), (1, 1)), 4: ((0, 0), (1, 2))}[mode]
    seen_result = False
    choosing = False
    live_again = False
    picks = [[], []]
    held = (0, 0, 0, 0)
    with trace.open('rb') as file:
        for frame in range(frames):
            data = file.read(0x200)
            if len(data) != 0x200:
                raise ValueError(f'incomplete trace at {frame}')
            held = owners.get(frame, held)
            step = int.from_bytes(data[0x104:0x106], 'big')
            if step == 12 and data[5] == expected:
                if result in ('time', 'draw') and data[8] != 0:
                    raise ValueError('round ended before the clock expired')
                seen_result = True
            if not seen_result:
                continue
            if step == 4:
                choosing = True
                for side, offset in enumerate((0x180, 0x1b0)):
                    count = data[offset + 3]
                    if count <= len(picks[side]):
                        continue
                    if count != len(picks[side]) + 1 or count > 2:
                        raise ValueError(f'pick skipped, side {side}, frame {frame}')
                    owner = order[side][count - 1]
                    if not (held[owner] & 0x3f0) or any(w & 0x3f0 for i, w in enumerate(held) if i != owner):
                        raise ValueError(f'pick {count} on side {side} did not come from P{owner+1} alone at {frame}')
                    picks[side].append(frame)
            elif choosing and step == 8 and data[0] == 8 and data[0x101] == 3:
                live_again = True
    counts = tuple(len(side) for side in picks)
    if not live_again or counts != ((2, 2) if everyone else (0, 2)):
        raise ValueError(f'result={seen_result}, selections={picks}, second human match={live_again}')
    state = ram.read_bytes()
    if any(state[base + 3] != 1 for base in (0x3000, 0x3400, 0x3800, 0x3c00)):
        raise ValueError('a fighter in the second match lost its human flag')
    return picks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', default=ROOT/'rom/mvsc', type=Path)
    parser.add_argument('--program', default=ROOT/'build/mvsc/mvsc', type=Path)
    parser.add_argument('--case', choices=CASES, action='append')
    args = parser.parse_args()
    for name in args.case or CASES:
        mode, result, start, everyone = CASES[name]
        with tempfile.TemporaryDirectory(prefix='mvc-rematch-') as tmp:
            work = Path(tmp)
            frames, owners = make_movie(work/'movie', mode, result, start)
            features = f'free_play,tag_team={mode},cross_fever'
            if everyone:
                features += ',cross_fever_reselect'
            run = subprocess.run([str(args.program.resolve()), '--rom', str(args.rom.resolve()), '--headless',
                                  '--frames', str(frames), '--features', features, '--movie', str(work/'movie'),
                                  '--trace', f'FF4000-FF41FF:{work}/trace', '--dump-ram', f'{frames-2}:{work}/ram',
                                  '--settings', str(work/'settings'), '--eeprom', str(work/'eeprom')],
                                 cwd=work, capture_output=True, text=True)
            if run.returncode:
                raise SystemExit(run.stderr)
            try:
                picks = verify(work/'trace', work/'ram', frames, owners, mode, result, everyone)
            except ValueError as exc:
                raise SystemExit(f'{name}: {exc}') from exc
            print(f'PASS {name}: {picks}, four human fighters in the second match.', flush=True)


if __name__ == '__main__':
    main()
