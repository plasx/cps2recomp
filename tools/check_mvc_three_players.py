#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morphus56K and contributors. Part of cps2recomp.
"""Play both three-person MVC layouts and observe each fighter's actual controller during Duo."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE_MOVIE = ROOT / 'games/mvsc/movies/cross_fever.txt'
FIRST_PROBE = 5922
# Direction and attack are independent on each controller. The second press includes a kick.
PROBES = [(0x011, 0x024, 0x042), (0x082, 0x108, 0x201)]
# FF3000 through FF410F contains the four fighter records, team timers and game step.
STRIDE = 0x1110
RECORDS = (0, 0x400, 0x800, 0xc00)


def make_movie(mode, output):
    lines = []
    for row in SOURCE_MOVIE.read_text().splitlines():
        if not row or row.startswith('#'):
            continue
        frame, *words = row.split()
        frame = int(frame)
        p1, p2, p3, p4 = (int(word, 16) for word in words)
        if frame >= 4200:
            break
        if frame >= 4172:
            # Allow the selected passive gauges to fill before their point owners perform QCB+HP+HK.
            frame += 1550
        if mode == 3:
            owners = (p1, p2 | p4, p3)
        else:
            owners = (p1 | p3, p2, p4)
        lines.append((frame, owners))
    for n, owners in enumerate(PROBES):
        lines.append((FIRST_PROBE + 32 * n, owners))
    lines.append((FIRST_PROBE + 64, (0, 0, 0)))
    output.write_text(''.join(f'{frame} ' + ' '.join(f'{word:04x}' for word in owners) + ' 0000\n'
                              for frame, owners in lines))


def verify(mode, path):
    count = 0
    reached_fight = False
    first_side_expired = False
    both_sides_expired = False
    probes = [False] * len(PROBES)
    with path.open('rb') as trace:
        for frame in range(7100):
            data = trace.read(STRIDE)
            if len(data) != STRIDE:
                raise ValueError(f'trace ended before frame {frame}')
            fighting = int.from_bytes(data[0x1104:0x1106], 'big') == 8 and data[0x1000] == 8
            reached_fight |= fighting
            if not fighting:
                continue
            active = data[0x1038] & 3
            if active == 3:
                count += 1
                for n, (p1, p2, p3) in enumerate(PROBES):
                    if not 0 <= frame - FIRST_PROBE - 32 * n < 32:
                        continue
                    expected = (p1, p2, p3, p2) if mode == 3 else (p1, p2, p1, p3)
                    # The CPS2 panel places kicks at bits 8 through 10.
                    panel = tuple((word & 0x7f) | ((word >> 7 & 7) << 8) for word in expected)
                    held = tuple(int.from_bytes(data[a+0xd0:a+0xd2], 'big') for a in RECORDS)
                    probes[n] |= held == panel
            if count and active != 3:
                left, right = (int.from_bytes(data[a:a+2], 'big') for a in (0x1034, 0x1036))
                roles = tuple(data[a + 0x281] for a in RECORDS)  # 0 = on point
                if active == 2 and left == 0 and right > 0 and roles == (1, 0, 0, 1):
                    first_side_expired = True
                if first_side_expired and active == 0 and left == right == 0 and roles == (1, 1, 0, 0):
                    both_sides_expired = True
    if not (reached_fight and count and all(probes) and both_sides_expired):
        raise ValueError(f'mode {mode}: reached fight={reached_fight}, both Duo frames={count}, '
                         f'owner probes={probes}, staggered expiry and point swaps={both_sides_expired}')
    print(f'PASS mode {mode}: ordinary selection and meter, {count} simultaneous Duo frames, '
          f'both controller patterns reached all four fighters, each expiry swapped only its own point')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, default=ROOT / 'rom/mvsc')
    parser.add_argument('--program', type=Path, default=ROOT / 'build/mvsc/mvsc')
    args = parser.parse_args()
    for mode in (3, 4):
        with tempfile.TemporaryDirectory(prefix='mvc-three-') as temp:
            work = Path(temp)
            movie, trace = work / 'inputs.txt', work / 'ram.bin'
            make_movie(mode, movie)
            result = subprocess.run([str(args.program.resolve()), '--rom', str(args.rom.resolve()), '--headless',
                                     '--frames', '7100', '--movie', str(movie), '--features',
                                     f'free_play,tag_team={mode},cross_fever,cross_fever_auto_gauge=1', '--trace', f'FF3000-FF410F:{trace}',
                                     '--settings', str(work / 'settings.ini'), '--eeprom', str(work / 'eeprom')],
                                    cwd=work, capture_output=True, text=True)
            if result.returncode:
                raise SystemExit(result.stderr or f'game exited {result.returncode}')
            verify(mode, trace)


if __name__ == '__main__':
    main()
