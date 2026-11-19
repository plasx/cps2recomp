#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morphus56K and contributors. Part of cps2recomp.
"""Keep a reserve player's tag chord out of the point player's team Hyper and Duo commands."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MOVIE = ROOT / 'games/mvsc/movies/ryu_contact_cross_fever.txt'
STRIDE = 0x1110
STARTERS = (0xff3000, 0xff3400)
PARTNERS = (0xff3800, 0xff3c00)


def controls(path, direction, split, paired=False):
    rows = {}
    for line in MOVIE.read_text().splitlines():
        if line and not line.startswith('#'):
            frame, *words = line.split()
            if int(frame) < 4100:
                rows[int(frame)] = tuple(int(w, 16) for w in words)
    neutral = (0, 0, 0, 0)
    rows[4100] = neutral
    # Ryu and Spider-Man build their own meter through heavy punches at a distance.
    for frame in range(4250, 5850, 30):
        rows[frame] = (0x40, 0x40, 0, 0)
        rows[frame + 4] = neutral
    left, right = (2, 1) if direction == 'back' else (1, 2)
    if direction == 'forward' and not split:
        right = 0  # isolate one native team Hyper from the opponent's super-freeze
    rows[6080] = (4, 4 if right else 0, 0, 0)
    rows[6084] = (4 | left, (4 | right) if right else 0, 0, 0)
    rows[6088] = (left, right, 0, 0)
    chord = 0x240
    rows[6089] = ((left, right, chord, chord) if split else
                  (left | chord, (right | chord) if right else 0, 0, 0))
    rows[6094] = neutral
    # The seed movie and generated moves are in fighter order: left/right point, left/right reserve.
    # Paired order swaps physical P2 and P3 for the entire movie, starting with the reserve joins.
    with path.open('w') as movie:
        for frame, words in sorted(rows.items()):
            if paired:
                words = (words[0], words[2], words[1], words[3])
            movie.write(f'{frame} ' + ' '.join(f'{word:04x}' for word in words) + '\n')


def observe(data, frame):
    row = memoryview(data)[frame * STRIDE:(frame + 1) * STRIDE]
    if len(row) != STRIDE or row[0x1000] != 8:
        raise ValueError(f'incomplete fight at {frame=}')
    levels = tuple(row[actor + 0x274] for actor in (0, 0x400))
    pointers = tuple(int.from_bytes(row[at:at + 4], 'big') for at in (0x10c8, 0x10d0))
    buttons = tuple(int.from_bytes(row[actor + 0xd0:actor + 0xd2], 'big') for actor in (0, 0x400))
    return levels, pointers, buttons, row[0x1038] & 3


def run(program, rom, features, direction, split, expected, order):
    with tempfile.TemporaryDirectory(prefix='mvc-bench-motion-') as tmp:
        work = Path(tmp)
        paired = order == 'paired'
        controls(work / 'input.txt', direction, split, paired)
        settings = 'free_play,tag_team=2' + (',tag_team_order=1' if paired else '') + features
        result = subprocess.run([program, '--rom', rom, '--headless', '--frames', '6350', '--movie', 'input.txt',
                                 '--features', settings, '--trace', 'FF3000-FF410F:trace',
                                 '--settings', 'settings.ini', '--eeprom', 'eeprom'], cwd=work, capture_output=True, text=True)
        if result.returncode:
            raise SystemExit(result.stderr or f'game exited {result.returncode}')
        data = (work / 'trace').read_bytes()
        before, first, _, _ = observe(data, 6087)
        after, on_point, _, duo = observe(data, 6330)
        if first != STARTERS or min(before) < 2:
            raise ValueError(f'both points must be ready with earned meter: {first=}, {before=}')
        if split:
            _, _, held, _ = observe(data, 6090)
            if any(word & 0x440 for word in held) or before != after or duo:
                raise ValueError(f'resting player supplied the point fighter with attacks: {held=}, {before=}, {after=}, {duo=}')
            if on_point != (PARTNERS if expected == 'tag' else STARTERS):
                raise ValueError(f'{on_point=} after {expected=}')
        elif expected == 'duo':
            if duo != 3 or after != (0, 0):
                raise ValueError(f'native point Duo failed: {duo=}, {after=}')
        elif duo or after != (before[0] - 2, before[1]):
            raise ValueError(f'native point team Hyper failed: {duo=}, {before=}, {after=}')
        mode = features.lstrip(',') or 'classic Tag team'
        motion = 'QCB' if direction == 'back' else 'QCF'
        print('PASS', order, mode, motion, 'bench chord' if split else 'point chord', expected, f'{before} -> {after}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--program', type=Path, default=ROOT / 'build/mvsc/mvsc')
    parser.add_argument('--rom', type=Path, default=ROOT / 'rom/mvsc')
    parser.add_argument('--order', choices=('dreamcast', 'paired'), action='append',
                        help='team order to test (default: both)')
    args = parser.parse_args()
    cases = (
        ('', 'back', True, 'tag'), ('', 'forward', True, 'tag'), ('', 'back', False, 'duo'),
        ('', 'forward', False, 'team Hyper'), (',cross_fever', 'back', True, 'point only'),
        (',cross_fever,cross_fever_partner_tag', 'back', True, 'tag'),
    )
    for order in args.order or ('dreamcast', 'paired'):
        for features, direction, split, expected in cases:
            run(args.program.resolve(), args.rom.resolve(), features, direction, split, expected, order)


if __name__ == '__main__':
    main()
