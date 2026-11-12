# Online reliability checks

The local checks need no deployed introduction service. Run the service checks without ROM data:

```sh
node --test online/test/*.test.js
```

These use the production Worker with in-memory Durable Object substitutes. They check admission and reservation
semantics, not Cloudflare deployment or real routers. The protocol checks exercise the production C++ networking
code with synthetic state and loopback packets:

```sh
tools/check_netplay.sh
```

SDL 3 development files, zlib, a C++20 compiler, Python 3, curl and pkg-config are required. Use `SDL_STATIC=1` when
linking the static SDL build. `make check-online-service` and `make check-netplay` run these same checks. They must
preserve departures when a poll fails, acknowledge only those included in the successful poll, and allow repeats. A host
reopening a match must send reconnect messages even if earlier end messages are lost or reordered. An intentional
host exit must still end the clients' sessions.

The C++ suite also sends actual cumulative input packets through loopback sockets: four different streams,
reordering, duplicates, a delayed peer, retransmission, host forwarding and rejection of a stream sent by the wrong
peer. It checks that prediction stalls at `kMostAhead` and resumes when missing controls arrive. Frame simulation
and confirmed hashes are checked separately by the gameplay runs below.

`make check-crossfever-reset` checks the production MVC gauge hook with a synthetic memory bus: boot clears every
prior counter phase, ordinary match steps retain it, and save/load restores it. It needs no ROM data and is a
boundary regression, not a substitute for playing through reset and rematch with the game.

## Gameplay and player placement

Build the game first, with your own ROM set. Each layout lists people per machine in arrival order, host first:

```sh
for layout in 1,1 2,2 2,1,1 1,2,1 1,1,2 1,1,1,1; do
    PEOPLE="$layout" GAME=mvsc tools/check_online.sh 3000 || exit 1
done
```

Each run compares confirmed state hashes between the machines and against replay of the recorded inputs. It also
checks controller ownership, names and chat delivery with injected delay, jitter and packet loss. The four-person
runs enable Cross Fever. With layout `1,1,2`, the final pair receives P3 and P4, because P1 and P2 were already
assigned; teams remain P1/P3 versus P2/P4. With `1,2,1`, the joining pair receives P2/P4 and the last arrival P3.

Also repeat after menu-hosted disconnects and rejoining, reset/rehost, and ordinary rematches. Check joining and
all six attack buttons for each owner, tags, simultaneous Duo, expiry and KO. Synthetic inputs and local processes
do not establish physical-controller or real internet acceptance.

## Internet acceptance still required

Record build, platform, people per machine, joining order, connection method, result, ping, stalls and desyncs.
Use four actual machines and cover:

| Network arrangement | Cases |
|---|---|
| One home network | LAN addresses; public-code joining through the same router |
| Separate home networks | Four independent connections; rotate the host |
| Restrictive/mobile network | Direct connection attempt; forwarded-port or VPN control |
| Uneven links | One slow peer, asymmetric delay, burst loss, packet reordering and duplication |
| Recovery | Every joining peer leaves in turn; reconnect and replace a peer; intentional host exit |

A pass includes a complete match and rematch, correct controller ownership, and matching confirmed state hashes.
Keep failure-stage evidence separate: lobby admission, address discovery, direct UDP handshake, game transfer,
simulation. Connection success and simulation correctness are different checks.

## Research guiding the next tests

- [Large-Scale Measurement of NAT Traversal (2026)](https://arxiv.org/abs/2604.12484) measures a different P2P system.
  Its separation of connection stages is useful here; its success rates are not measurements of this port.
- [Peer-to-Peer Communication Across NATs](https://arxiv.org/abs/cs/0603074) explains why address discovery and hole
  punching depend on router behavior. Use the network matrix above before making connectivity claims.
- [Formalizing Rollback Netcodes (OPODIS 2025)](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.OPODIS.2025.11)
  studies a client-server model. Applying its reasoning to this host-forwarded simulation suggests tests that
  confirmed frames contain all required real inputs, prediction stops at its limit, and replay never needs a state
  older than the retained history. These are proposed applications, not a proof supplied by that paper.

Relay fallback remains a separate design: establish direct-connection failure reasons first, then evaluate relay
latency, capacity and operating cost. Neither a relay nor real internet acceptance is provided by the local tests.

## Local verification, November 11, 2026

On an arm64 Mac, with the interpreter build of MVC, Apple Clang 21, SDL 3.4.20 and Node 25.9.0:

- `make check`: repository audit, shared-source check, example build, and launcher startup/screenshots passed.
- 22 Worker tests and seven production C++ protocol test groups passed. Temporarily restoring either early removal
  clearing or the reopening host's terminal goodbye made its regression test fail.
- All six layouts above passed 3,000-frame runs on the final build, with matching common confirmed-frame hashes,
  offline replay, assignments, names and chat. These were local processes with simulated network faults.
- The Cross Fever reset test passed every counter phase, disabled modes, ordinary-match continuity and save/load.
  A separate game-level boundary comparison replayed the existing Cross Fever movie from two pre-boot states
  differing only in the saved gauge phase: all 4,500 frame hashes and meter samples matched after the fix. Before
  the fix they diverged; the patched cold start also matched the original cold start. This is a modified serialized
  counter fixture, not natural menu-reset or remote-play acceptance, and it does not establish a prior peer desync.

Other games, physical controllers, real internet matches and Windows/Linux runtime execution remain separate
validation. CI is configured to run the service/reset tests and Linux protocol tests; the local results above do
not stand in for a completed CI run or a performance benchmark.
