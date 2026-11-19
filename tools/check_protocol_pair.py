#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
"""Check direct-UDP player compatibility in both directions with two real MVC builds."""
import argparse
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time


def play(host_game, other_game, rom, work):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as available:
        available.bind(('127.0.0.1', 0))
        port = str(available.getsockname()[1])
    base = ['--rom', str(rom), '--headless', '--frames', '90', '--net-service', 'off']
    host_log = work / 'host.log'
    with host_log.open('w') as log, subprocess.Popen(
        [str(host_game), *base, '--host', port, '--settings', 'host.ini', '--eeprom', 'host.ee'],
        cwd=work, stdout=log, stderr=log,
    ) as host:
        try:
            deadline = time.monotonic() + 20
            while 'online: Waiting for' not in host_log.read_text():
                if host.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('Host did not open its room: ' + host_log.read_text())
                time.sleep(0.05)
            join = [*base, '--join', '127.0.0.1:' + port, '--settings', 'guest.ini', '--eeprom', 'guest.ee']
            wrong = subprocess.run([str(other_game), *join], cwd=work, capture_output=True, text=True, timeout=30)
            if 'different versions' not in wrong.stderr or 'the match is on:' in wrong.stderr:
                raise RuntimeError('Expected the other protocol to be refused: ' + wrong.stderr)
            good = subprocess.run([str(host_game), *join], cwd=work, capture_output=True, text=True, timeout=40)
            host.wait(timeout=15)
            text = host_log.read_text()
            if good.returncode or host.returncode or 'online: the match is on:' not in good.stderr or 'online: turned away a player' not in text:
                raise RuntimeError('The compatible player did not get the vacant seat: ' + text + good.stderr)
            handshake = r'online: (\S+ \S+), protocol (\d+), game signature (\w+)'
            own, other = (re.search(handshake, raw) for raw in (text, wrong.stderr))
            if not own or not other or own[2] == other[2] or (own[1], own[3]) != (other[1], other[3]):
                raise RuntimeError('Expected the same reported game/version and ROM signature on different protocols')
            print(f'PASS: protocol {own[2]} refused {other[2]} and then completed with {own[2]}, {own[1]}, game signature {own[3]}')
        finally:
            if host.poll() is None:
                host.kill()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--program', type=Path, required=True)
    parser.add_argument('--previous', type=Path, required=True)
    parser.add_argument('--rom', type=Path, required=True)
    args = parser.parse_args()
    current, prior, rom = (p.resolve() for p in (args.program, args.previous, args.rom))
    for host, guest in ((current, prior), (prior, current)):
        with tempfile.TemporaryDirectory(prefix='cps2-protocol-pair-') as work:
            play(host, guest, rom, Path(work))


if __name__ == '__main__':
    main()
