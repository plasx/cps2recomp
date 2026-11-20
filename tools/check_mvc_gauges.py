#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Part of cps2recomp.
"""At the default board speed, check idle meter, attack gains, Duo payment and four-player control."""
import argparse
import configparser
from pathlib import Path
import subprocess
import tempfile

from check_mvc_team_order import panel

ROOT = Path(__file__).resolve().parents[1]
SEED = ROOT / 'games/mvsc/movies/ryu_contact_cross_fever.txt'
LOW, HIGH = 0xff3000, 0xff4040
FRAMES = 6560
IDLE = range(4250, 4500)
# Distinct physical inputs for the two points and their reserves after both teams pay for Duo.
PROBES = {6460: (0x011, 0x024, 0x042, 0x088), 6500: (0x082, 0x108, 0x201, 0x014)}


def make_movie(path):
    # Use the existing four human picks and ordinary attacks, then stop both teams in neutral.
    lines = [row for row in SEED.read_text().splitlines() if row and not row.startswith('#')
             and int(row.split()[0]) < 4100]
    lines.append('4100 0000 0000 0000 0000')
    for f in range(4500, 6100, 30):
        lines += [f'{f} 0040 0040 0000 0000', f'{f+4} 0000 0000 0000 0000']
    lines += ['6280 0004 0004 0000 0000', '6284 0006 0005 0000 0000',
              '6288 0242 0241 0000 0000', '6294 0000 0000 0000 0000']
    for frame, words in PROBES.items():
        lines += [f'{frame} ' + ' '.join(f'{word:04x}' for word in words),
                  f'{frame + 30} 0000 0000 0000 0000']
    path.write_text('\n'.join(lines)+'\n')


def read(row):
    def gauge(base):
        # Gauge levels and the current bar's fraction, in the game's units (0x90 per full bar).
        at = base - LOW
        return row[at+2] * 0x90 + int.from_bytes(row[at:at+2], 'big')
    held = tuple(int.from_bytes(row[at+0xd0:at+0xd2], 'big') for at in (0, 0x400, 0x800, 0xc00))
    return gauge(0xff3272), gauge(0xff3672), row[0xff4038-LOW] & 3, row[0xff4000-LOW], held


def run(program, rom, passive):
    with tempfile.TemporaryDirectory(prefix='mvc-gauge-') as name:
        work = Path(name)
        make_movie(work/'input.txt')
        options = 'free_play,tag_team=2,cross_fever' + (',cross_fever_auto_gauge=1' if passive else '')
        cmd = [program, '--rom', rom, '--headless', '--frames', str(FRAMES), '--features', options,
               '--movie', 'input.txt', '--trace', f'{LOW:X}-{HIGH:X}:trace', '--eeprom', 'eeprom',
               '--settings', 'settings.ini']
        result = subprocess.run(cmd, cwd=work, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stderr)
        with (work/'trace').open('rb') as fp:
            rows = [read(fp.read(HIGH-LOW+1)) for _ in range(FRAMES)]
        if any(rows[f][3] != 8 or rows[f][2] for f in IDLE):
            raise ValueError('idle check must take place during an ordinary fight')
        before, after, charged = rows[IDLE.start][:2], rows[IDLE.stop-1][:2], rows[6287][:2]
        for side in (0, 1):
            if passive:
                if after[side] <= before[side]:
                    raise ValueError(f'passive gauge {side} did not advance during idle: {before} -> {after}')
            elif any(rows[f][side] != before[side] for f in IDLE):
                raise ValueError(f'fighting-only gauge {side} grew during idle')
            if charged[side] < 2*0x90 or (not passive and charged[side] <= after[side]):
                raise ValueError(f'gauge {side} did not earn enough through play: {after} -> {charged}')
        duo = next((n for n in range(6288, FRAMES) if rows[n][2] == 3), None)
        if duo is None or any(x >= 0x90 for x in rows[duo][:2]):
            raise ValueError(f'native simultaneous Duo did not spend both full gauges: {duo=}, {charged=}')
        for frame, words in PROBES.items():
            expected = tuple(map(panel, words))
            if not any(row[2] == 3 and row[3] == 8 and row[4] == expected for row in rows[frame:frame+30]):
                raise ValueError(f'four-player Duo input mismatch in frames {frame}..{frame+29}, expected {expected}')
        print(f'PASS {options}: idle {IDLE.start}..{IDLE.stop-1} {before} -> {after}, '
              f'after attacks {charged} at 6287, both Duos active at {duo} with {rows[duo][:2]}, '
              'two distinct inputs per owner reached all four fighters', flush=True)


def check_saved_choices(program, rom):
    released, preview, current = 'cross_fever_gauge', 'cross_fever_earned_meter', 'cross_fever_auto_gauge'
    menu = subprocess.check_output([program, '--list-features'], text=True)
    gauge_rows = [r.split('\t') for r in menu.splitlines() if r.split('\t')[0] in (released, preview, current)]
    if len(gauge_rows) != 1 or gauge_rows[0][0] != current or gauge_rows[0][-1] != 'Only by fighting|Fill by themselves':
        raise ValueError('expected one selector with a fresh fighting-only default')

    cases = [({key: str(old)}, 1-old) for key in (released, preview) for old in (0, 1)]
    cases += [({released: '0', preview: '1'}, 1), ({current: '0', released: '0'}, 0)]
    for saved, expected in cases:
        with tempfile.TemporaryDirectory(prefix='mvc-old-gauge-') as name:
            work = Path(name)
            prefs = work / 'settings.ini'
            prefs.write_text('# keep my bindings\n[features]\npreset=custom\n' +
                             ''.join(f'{key}={value}\n' for key, value in saved.items()) +
                             '[other]\nmy_value=preserved\n')
            cmd = [program, '--rom', rom, '--headless', '--frames', '1', '--settings', prefs, '--eeprom', work/'eeprom']
            subprocess.run(cmd, cwd=work, check=True, capture_output=True)
            upgraded = prefs.read_text()
            config = configparser.ConfigParser()
            config.read_string(upgraded)
            if (config.getint('features', current) != expected or
                    any(config.get('features', key, fallback='') for key in (released, preview)) or
                    config.get('other', 'my_value') != 'preserved' or '# keep my bindings' not in upgraded):
                raise ValueError(f'gauge migration changed the wrong settings: {saved=}, {expected=}')
            subprocess.run(cmd, cwd=work, check=True, capture_output=True)
            if prefs.read_text() != upgraded:
                raise ValueError('gauge migration ran again on the upgraded settings')
    print('PASS one gauge selector and six saved-preset upgrades, each reopened unchanged', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--program', type=Path, default=ROOT/'build/mvsc/mvsc')
    parser.add_argument('--rom', type=Path, default=ROOT/'rom/mvsc')
    args = parser.parse_args()
    check_saved_choices(args.program.resolve(), args.rom.resolve())
    for passive in (False, True):
        run(args.program.resolve(), args.rom.resolve(), passive)
