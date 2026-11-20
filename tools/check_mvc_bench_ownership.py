#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
"""Drive MVC with ordinary inputs through hitstun, tag entry/release, and two-player taunts during Duo."""
import argparse
from pathlib import Path
import subprocess
import tempfile

from check_mvc_rematch import make_movie
from check_mvc_tag_motion import controls, MOVIE

ROOT = Path(__file__).resolve().parents[1]
NEUTRAL = (0, 0, 0, 0)
POINT = (0xff3000, 0xff3400)
RESERVE = (0xff3800, 0xff3c00)
STRIDE = 0x1110


def read_movie(path, before):
    rows = {}
    for line in path.read_text().splitlines():
        if line and not line.startswith('#'):
            frame, *words = line.split()
            if int(frame) < before:
                rows[int(frame)] = tuple(int(word, 16) for word in words)
    return rows


def play(work, args, rows, frames, features):
    with (work / 'input.txt').open('w') as movie:
        for frame, words in sorted(rows.items()):
            movie.write(f'{frame} ' + ' '.join(f'{word:04x}' for word in words) + '\n')
    # The hit, recovery and tag assertions use the exact every-clock frames recorded for this movie.
    result = subprocess.run([args.program, '--rom', args.rom, '--headless', '--cpu', 'interp', '--frames', str(frames),
                             '--movie', 'input.txt', '--features', 'slowdown=1,free_play,tag_team=2' + features,
                             '--trace', 'FF3000-FF410F:trace', '--settings', 'settings.ini', '--eeprom', 'eeprom'],
                            cwd=work, capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr or f'game exited {result.returncode}')
    data = (work / 'trace').read_bytes()
    if len(data) != frames * STRIDE:
        raise ValueError(f'incomplete RAM trace: {len(data)} bytes')
    return memoryview(data)


def record(data, frame):
    row = data[frame * STRIDE:(frame + 1) * STRIDE]
    if row[0x1000] != 8:
        raise ValueError(f'outside the live fight at {frame=}')
    return row


def points(row):
    return tuple(int.from_bytes(row[i:i + 4], 'big') for i in (0x10c8, 0x10d0))


def hitstun(work, args, hold):
    # Preserve the four seat admissions and the left player's approach to the first hit.
    seed = work / 'left-ko.txt'
    make_movie(seed, 2, 'left', False)
    rows = read_movie(seed, 4440)
    rows[4440] = NEUTRAL
    end = 4465 if hold else 4435
    current = NEUTRAL
    for frame in sorted(rows.keys() | {4430, end}):
        current = rows.get(frame, current)
        if frame >= 4430:
            rows[frame] = (*current[:3], 0x240 if frame < end else 0)
    data = play(work, args, rows, 4570, ',cross_fever,cross_fever_partner_tag')
    hit = record(data, 4424)
    if hit[0x4b0] != 0x36 or int.from_bytes(hit[0x670:0x672], 'big') != 121:
        raise ValueError('expected a native hit on the right point before the reserve pressed HP+HK')
    for frame in range(4430, 4570):
        row = record(data, frame)
        expected = (POINT[0], RESERVE[1]) if hold and frame >= 4469 else POINT
        if points(row) != expected or (frame == 4455 and (row[0x4b0] == 0x40) != hold):
            raise ValueError(f'wrong tag action or point ownership during recovery at {frame=} ({hold=})')
    print('PASS hitstun:', 'held request entered tag at 4455, changed point at 4469' if hold else
          'release at 4435 cancelled the request', flush=True)


def tag_release(work, args, rule):
    rows = read_movie(MOVIE, 4100)
    rows.update({4100: NEUTRAL, 4200: (0x40, 0x40, 0, 0), 4202: (0x40, 0x40, 0x240, 0x240),
                 4204: (0, 0, 0x240, 0x240), 4330: NEUTRAL})
    features = {'Classic': '', 'Point': ',cross_fever', 'Either': ',cross_fever,cross_fever_partner_tag'}
    data = play(work, args, rows, 4440, features[rule])
    for frame in range(4195, 4440):
        expected = RESERVE if rule != 'Point' and frame >= 4215 else POINT
        if points(record(data, frame)) != expected:
            raise ValueError(f'unexpected tag at {frame=} ({rule=})')
    print(f'PASS {rule}:', 'bench chord ignored' if rule == 'Point' else
          'both tags completed at 4215, release at 4330 did not tag back', flush=True)


def duo_taunt(work, args, case):
    seed = work / 'both-duos.txt'
    controls(seed, 'back', False)
    rows = read_movie(seed, 6350)
    # P1 is left point, P3 is left reserve. Both are active after the point owners' complete QCB+HP+HK.
    probes = {'same owner': (0, 0, 0x410, 0), 'split owners': (0x400, 0, 0x10, 0),
              'extra direction': (0, 0, 0x411, 0)}
    rows.update({6350: probes[case], 6550: NEUTRAL})
    data = play(work, args, rows, 6610, ',cross_fever,cross_fever_taunt=1')
    taunts = []
    for frame in range(6330, 6580):
        row = record(data, frame)
        if (row[0x1038] & 3) != 3 or points(row) != POINT or row[0xb0] == 0x2a:
            raise ValueError(f'Duo or point control changed at {frame=}')
        if row[0x8b0] == 0x2a:
            taunts.append(frame)
    if taunts != (list(range(6350, 6550)) if case == 'same owner' else []):
        raise ValueError(f'wrong P3 taunt with {case}: {taunts[:8]}')
    print(f'PASS Duo taunt, {case}: {len(taunts)} P3 taunt frames', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--program', type=Path, default=ROOT / 'build/mvsc/mvsc')
    parser.add_argument('--rom', type=Path, default=ROOT / 'rom/mvsc')
    parser.add_argument('--suite', choices=('hitstun', 'tag', 'taunt'))
    args = parser.parse_args()
    args.program, args.rom = args.program.resolve(), args.rom.resolve()
    cases = {'hitstun': (hitstun, (True, False)), 'tag': (tag_release, ('Classic', 'Either', 'Point')),
             'taunt': (duo_taunt, ('same owner', 'split owners', 'extra direction'))}
    for suite in [args.suite] if args.suite else cases:
        test, variants = cases[suite]
        for variant in variants:
            with tempfile.TemporaryDirectory(prefix='mvc-bench-') as folder:
                test(Path(folder), args, variant)


if __name__ == '__main__':
    main()
