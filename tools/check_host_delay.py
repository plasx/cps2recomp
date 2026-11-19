#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
"""Check the delay actually negotiated by two local MVC processes with isolated launcher/game settings."""
import argparse
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def play(work, program, rom, global_delay, game_delay, cli, expected):
    # An untouched dropdown uses index zero; numeric choices are stored with an offset of one.
    (work / 'game.ini').write_text(f'[features]\nonline_host_delay={0 if game_delay is None else game_delay + 1}\n')
    prefs = work / 'user/CPS-2 Recomp/launcher.ini'
    prefs.write_text('[online]\n' + ('' if global_delay is None else f'delay={global_delay}\n'))
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as available:
        available.bind(('127.0.0.1', 0))
        port = str(available.getsockname()[1])
    base = [str(program), '--rom', str(rom), '--headless', '--frames', '90', '--net-service', 'off']
    host_args = base + ['--host', port, '--settings', str(work / 'game.ini'), '--eeprom', str(work / 'host.eeprom'), *cli]
    guest_args = base + ['--join', '127.0.0.1:' + port, '--settings', str(work / 'guest.ini'), '--eeprom', str(work / 'guest.eeprom')]
    with subprocess.Popen(host_args, cwd=work, stderr=subprocess.PIPE, text=True) as host:
        try:
            guest = subprocess.run(guest_args, cwd=work, capture_output=True, text=True, timeout=50)
            _, host_log = host.communicate(timeout=10)
        finally:
            if host.poll() is None:
                host.kill()
    pattern = rf'the match is on: .*\({expected} frames? of delay\)'
    if host.returncode or guest.returncode or not all(re.search(pattern, log) for log in (host_log, guest.stderr)):
        raise RuntimeError(f'expected {expected} frames on both peers\n{host_log}\n{guest.stderr}')
    print(f'PASS {global_delay=} {game_delay=} {cli=} → {expected} frames on both peers', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--program', type=Path, default=ROOT / 'build/mvsc/mvsc')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='cps2-host-delay-') as tmp:
        work = Path(tmp)
        # Portable mode lets each case supply a real launcher.ini without touching the player's settings.
        program = Path(shutil.copy2(args.program.resolve(), work))
        for dll in args.program.resolve().parent.glob('*.dll'):
            shutil.copy2(dll, work)
        (work / 'portable.txt').touch()
        (work / 'user/CPS-2 Recomp').mkdir(parents=True)
        for case in (
            (None, None, [], 2),
            (5, None, [], 5),
            (0, None, [], 0),
            (5, None, ['--launcher-net-delay', '5'], 5),
            (5, 3, ['--launcher-net-delay', '5'], 3),
            (5, 3, ['--net-delay', '0', '--launcher-net-delay', '5'], 0),
        ):
            play(work, program, args.rom.resolve(), *case)


if __name__ == '__main__':
    main()
