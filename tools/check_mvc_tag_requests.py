#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Morphus56K and contributors. Part of cps2recomp.
"""Check the Cross Fever tag option from normal joining, selection and HP+HK on both teams; also check the selectable Dreamcast taunt."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
STARTERS = (0xff3000, 0xff3400)
PARTNERS = (0xff3800, 0xff3c00)


def selection():
    rows = []
    for line in (ROOT / 'games/mvsc/movies/cross_fever.txt').read_text().splitlines():
        if line and not line.startswith('#') and int(line.split()[0]) < 2800:
            rows.append(line)
    return rows


def write_movie(path):
    rows = selection()
    # Both second players confirm selection. Repeated first-player presses advance the intro.
    for frame in (*range(2800, 3490, 120), 4010, 4250, 4520, 4850):
        bench = frame not in (4250, 4850)
        rows.append(f"{frame} " + ("0000 0000 0240 0240" if bench else "0240 0240 0000 0000"))
        rows.append(f"{frame + 5} 0000 0000 0000 0000")
    path.write_text('\n'.join(rows) + '\n')


def verify(trace, either):
    stride = 0x10b
    data = trace.read_bytes()
    if len(data) != 5050 * stride:
        raise ValueError('incomplete run')
    # One live round throughout. FF40C8/FF40D0 are the game's two on-point record pointers.
    expected = (STARTERS, PARTNERS, STARTERS) if either else (STARTERS, STARTERS, PARTNERS)
    for frame, owners in zip((4510, 4690, 5020), expected):
        row = data[(frame - 1) * stride:frame * stride]
        actual = tuple(int.from_bytes(row[i:i + 4], 'big') for i in (0xc8, 0xd0))
        if row[0] != 8 or row[0x104:0x106] != b'\x00\x08' or actual != owners:
            raise ValueError(f'{frame=}, {either=}, {actual=}, expected {owners} in a live round')


def check_taunt(program, rom):
    # Reach the same fighting round using ordinary selects. Give each attack time to finish before Start.
    rows = selection()
    for frame in range(2800, 3490, 120):
        rows += [f'{frame} 0000 0000 0240 0240', f'{frame + 5} 0000 0000 0000 0000']
    for frame, word in [(4000, 0), (4150, 0x400), (4350, 0), (4400, 0x10), (4440, 0x410),
                        (4660, 0), (4700, 0x20), (4740, 0x420), (4940, 0),
                        (5000, 0x11), (5040, 0x411), (5200, 0)]:
        rows.append(f'{frame} {word:04x} 0000 0000 0000')
    for binding in (0, 1, 2):
        with tempfile.TemporaryDirectory(prefix='mvc-taunt-') as tmp:
            work = Path(tmp)
            movie, trace = work / 'input.txt', work / 'actions.bin'
            movie.write_text('\n'.join(rows) + '\n')
            run = subprocess.run([str(program), '--rom', str(rom), '--headless', '--frames', '5210',
                                  '--features', f'free_play,tag_team=2,cross_fever,cross_fever_taunt={binding}',
                                  '--movie', str(movie), '--trace', f'FF30B0-FF30B0:{trace}',
                                  '--settings', str(work / 'settings.ini'), '--eeprom', str(work / 'eeprom')],
                                 cwd=work, capture_output=True, text=True)
            if run.returncode:
                raise SystemExit(run.stderr or f'game exited {run.returncode}')
            actions = trace.read_bytes()
            if len(actions) != 5210:
                raise ValueError(f'incomplete taunt trace ({len(actions)} frames)')
            windows = [(4155, 4340), (4445, 4650), (4745, 4930), (5045, 5190)]
            starts = [sum(actions[f] == 0x2a and actions[f-1] != 0x2a for f in range(lo, hi))
                      for lo, hi in windows]
            if binding == 0:
                valid = starts[0] >= 2
            else:
                valid = starts[binding] >= 2 and all(n == 0 for i, n in enumerate(starts) if i != binding)
            if not valid:
                raise ValueError(f'taunt binding {binding}, entries in Start/LP/MP/directed-LP windows: {starts}')
            print(f'PASS taunt binding {binding}: entries in Start/LP/MP/directed-LP windows {starts}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, default=ROOT / 'rom/mvsc')
    parser.add_argument('--program', type=Path, default=ROOT / 'build/mvsc/mvsc')
    args = parser.parse_args()
    for either in (False, True):
        with tempfile.TemporaryDirectory(prefix='mvc-tag-') as temp:
            work = Path(temp)
            movie, trace = work / 'input.txt', work / 'teams.bin'
            write_movie(movie)
            features = 'free_play,tag_team=2,cross_fever' + (',cross_fever_partner_tag' if either else '')
            run = subprocess.run([str(args.program.resolve()), '--rom', str(args.rom.resolve()), '--headless',
                                  '--frames', '5050', '--features', features, '--movie', str(movie),
                                  '--trace', f'FF4000-FF410A:{trace}', '--settings', str(work / 'settings.ini'),
                                  '--eeprom', str(work / 'eeprom')], cwd=work, capture_output=True, text=True)
            if run.returncode:
                raise SystemExit(run.stderr or f'game exited {run.returncode}')
            verify(trace, either)
            print('PASS', 'Either: each team accepted a bench tag and a return tag' if either else
                  'Point: each team ignored its bench and accepted its point tag')

    check_taunt(args.program.resolve(), args.rom.resolve())


if __name__ == '__main__':
    main()
