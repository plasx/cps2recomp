#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
"""Play the same MVC fight using both four-human team orders, observing selection, Duo and tags."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
# Physical controller for each fighter record: left point, right point, left reserve, right reserve.
OWNERS = {'dreamcast': (0, 1, 2, 3), 'paired': (0, 2, 1, 3)}
RECORDS = (0, 0x400, 0x800, 0xc00)
STARTERS = (0xff3000, 0xff3400)
PARTNERS = (0xff3800, 0xff3c00)
NEUTRAL = (0, 0, 0, 0)
# These are physical streams in both profiles. Each probe distinguishes all four pads.
PROBES = {5922: (0x011, 0x024, 0x042, 0x088), 5954: (0x082, 0x108, 0x201, 0x014)}
# Each input poll below holds just one attack per controller. Rotate through LP, MP, HP,
# LK, MK and HK; all four controllers press distinct attacks on every such poll.
ATTACKS = (0x10, 0x20, 0x40, 0x80, 0x100, 0x200)
for phase in range(len(ATTACKS)):
    PROBES[6010 + 16 * phase] = tuple(ATTACKS[(phase + owner) % 6] for owner in range(4))
FRAMES, STRIDE = 7780, 0x11b4


def movie(path, owners):
    rows = {0: NEUTRAL}

    def assign(frame, roles):
        pads = [0] * 4
        for owner, value in zip(owners, roles):
            pads[owner] = value
        rows[frame] = tuple(pads)

    def pulse(frame, actor, value):
        words = [0] * 4
        words[actor] = value
        assign(frame, words)
        rows[frame + 6] = NEUTRAL

    # Join from each team's reserve. Press one player's direction+attack at a time so a
    # confirmed first or second pick is attributable to exactly one physical controller.
    pulse(1200, 2, 0x400)
    pulse(1260, 3, 0x400)
    for start in range(1400, 4000, 112):
        for actor, command in enumerate((0x11, 0x12, 0x21, 0x22)):
            pulse(start + 24 * actor, actor, command)
    # The selected passive gauges fill, then both point players perform QCB+HP+HK.
    for frame, words in ((5750, (4, 4, 0, 0)), (5754, (6, 5, 0, 0)),
                         (5758, (0x242, 0x241, 0, 0)), (5764, NEUTRAL)):
        assign(frame, words)
    rows.update(PROBES)
    rows[5986] = NEUTRAL
    rows[6106] = NEUTRAL
    # Both first picks are benched after expiry. A point-only tag request must ignore
    # their chord. Then send fresh isolated HP+HK requests, one controller at a time.
    assign(7130, (0x240, 0x240, 0, 0))
    rows[7136] = NEUTRAL
    for frame, actor in ((7230, 3), (7350, 2), (7510, 0), (7640, 1)):
        pulse(frame, actor, 0x240)
    path.write_text(''.join(f'{f} ' + ' '.join(f'{p:04x}' for p in pads) + '\n'
                            for f, pads in sorted(rows.items())))
    return rows


def panel(pad):
    return (pad & 0x7f) | ((pad >> 7 & 7) << 8)


def verify(path, owners, inputs):
    picks = [[], []]
    probes = set()
    selected_ids = [[], []]
    expiries = []
    both_active = 0
    old_active = 0
    held = NEUTRAL
    expected_points = {7190: PARTNERS, 7300: (PARTNERS[0], STARTERS[1]), 7420: STARTERS,
                       7580: (PARTNERS[0], STARTERS[1]), 7740: PARTNERS}
    with path.open('rb') as trace:
        for frame in range(FRAMES):
            data = trace.read(STRIDE)
            if len(data) != STRIDE:
                raise ValueError(f'trace stopped at frame {frame}')
            held = inputs.get(frame, held)
            step = int.from_bytes(data[0x1104:0x1106], 'big')
            if step == 4:
                for side, offset in enumerate((0x1183, 0x11b3)):
                    count = data[offset]
                    if count <= len(picks[side]):
                        continue
                    if count != len(picks[side]) + 1 or count > 2:
                        raise ValueError(f'skipped pick on side {side} at frame {frame}')
                    owner = owners[side + 2 * (count - 1)]
                    pressed = [i + 1 for i, pad in enumerate(held) if pad & 0x3f0]
                    if pressed != [owner + 1]:
                        raise ValueError(f'frame {frame}: side {side}, pick {count}, expected P{owner + 1}, '
                                         f'controllers pressing attack {pressed}')
                    choice = 0x10b0 + 4 * side + 2 * (count - 1)
                    selected_ids[side].append(int.from_bytes(data[choice:choice + 2], 'big'))
                    picks[side].append((frame, owner + 1))
            live = step == 8 and data[0x1000] == 8
            if frame in expected_points:
                point = tuple(int.from_bytes(data[o:o+4], 'big') for o in (0x10c8, 0x10d0))
                if not live or point != expected_points[frame]:
                    raise ValueError(f'frame {frame}: wrong point actors after tag, {point=}')
            if not live:
                continue
            active = data[0x1038] & 3
            if active == 3:
                if both_active == 0:
                    if list(map(len, selected_ids)) != [2, 2]:
                        raise ValueError(f'Duo began without four selected fighters: {selected_ids=}')
                    chosen = tuple(selected_ids[side][pick] for pick in range(2) for side in range(2))
                    actual_ids = tuple(int.from_bytes(data[r + 0x52:r + 0x54], 'big') for r in RECORDS)
                    if len(set(chosen)) != 4 or actual_ids != chosen:
                        raise ValueError(f'Duo fighter identities differ from the four selections: {actual_ids=}, {chosen=}')
                both_active += 1
                for event, physical in PROBES.items():
                    if event <= frame < event + 32 and held == physical:
                        expected = tuple(panel(physical[owner]) for owner in owners)
                        observed = tuple(int.from_bytes(data[r+0xd0:r+0xd2], 'big') for r in RECORDS)
                        if observed == expected:
                            probes.add(event)
            if both_active and old_active != active and active in (2, 0):
                timers = tuple(int.from_bytes(data[o:o+2], 'big') for o in (0x1034, 0x1036))
                if timers[0] != 0 or (timers[1] > 0) != (active == 2):
                    raise ValueError(f'frame {frame}: Duo timer and activation disagree, {timers=}, {active=}')
                expiries.append((active, frame))
            old_active = active
    if list(map(len, picks)) != [2, 2] or probes != PROBES.keys() or [bits for bits, _ in expiries] != [2, 0]:
        raise ValueError(f'incomplete ownership trace: {picks=}, {both_active=}, {probes=}, {expiries=}')
    return f'confirmed picks (frame, player) {picks}, {both_active} simultaneous Duo frames, two direction/attack probes and all six attacks per player, expiries {expiries}, four isolated normal tags'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--program', type=Path, default=ROOT / 'build/mvsc/mvsc')
    parser.add_argument('--rom', type=Path, default=ROOT / 'rom/mvsc')
    parser.add_argument('--order', choices=OWNERS, action='append')
    args = parser.parse_args()
    for order in args.order or OWNERS:
        with tempfile.TemporaryDirectory(prefix='mvc-team-order-') as tmp:
            work = Path(tmp)
            inputs = movie(work / 'input.txt', OWNERS[order])
            features = 'free_play,tag_team=2,cross_fever,cross_fever_auto_gauge=1' + (',tag_team_order=1' if order == 'paired' else '')
            result = subprocess.run([args.program.resolve(), '--rom', args.rom.resolve(), '--headless', '--frames',
                                     str(FRAMES), '--movie', 'input.txt', '--features', features, '--settings', 'settings.ini',
                                     '--eeprom', 'eeprom', '--trace', 'FF3000-FF41B3:trace'], cwd=work, capture_output=True, text=True)
            if result.returncode:
                raise SystemExit(result.stderr or f'game exited {result.returncode}')
            print('PASS', order, verify(work / 'trace', OWNERS[order], inputs), flush=True)


if __name__ == '__main__':
    main()
