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

The merged online features can be combined with each layout. For example:

```sh
WATCH=1 FIRST_TO=1 PEOPLE=1,1,1,1 GAME=mvsc tools/check_online.sh 3000
```

`FIRST_TO=0|1|2` checks Casual/FT5/FT10 negotiation: every joining machine asks for a different format, and every
playing machine must use the host's and agree on any set-score events through the shared confirmed frame. A short
run may contain no scored game; it does not establish completion of a whole FT5/FT10 set. `WATCH=1` joins a viewer
after play begins, compares at least 100 of its absolute frame hashes against the host, and verifies its attempted
chat never reaches the players. The viewer uses synthetic inputs too; its playback must still match the match.

`--watch CODE` (or a direct address) now starts a spectator. Memory-write diagnostics use
`--watch-writes LOW[-HIGH]` instead, with hexadecimal 68000 addresses. Update older diagnostic commands that
used `--watch` for memory writes; that spelling now belongs to online viewing.

Each run compares confirmed state hashes between the machines and against replay of the recorded inputs. It also
checks controller ownership, names and chat delivery with injected delay, jitter and packet loss. The four-person
runs enable Cross Fever. The following placements assume the default Dreamcast order. With layout `1,1,2`, the final pair receives P3 and P4, because P1 and P2 were already
assigned; teams remain P1/P3 versus P2/P4. With `1,2,1`, the joining pair receives P2/P4 and the last arrival P3.

Also repeat after menu-hosted disconnects and rejoining, reset/rehost, and ordinary rematches. Check joining and
all six attack buttons for each owner, tags, simultaneous Duo, expiry and KO. Synthetic inputs and local processes
do not establish physical-controller or real internet acceptance.

## November 20 keyboard and controller checks

With left Alt assigned to P1 medium punch and B assigned to P2 light punch, the SDL keyboard sampler returned
both attacks in the same poll. The regression also exercises right Alt, capture, and press/release across a wait.
Run `make check-netplay check-gamepad-slots` to repeat the sampler and virtual-controller tests.

In a local MVC match on macOS, the host enabled **Keyboard only**. The guest left it off and held native light
punch plus a mapped medium punch. The guest received `0030`. Switching the guest to keyboard only gave `0020`.
Right-stick and Guide events already waiting in SDL opened neither chat nor the menu. T and Esc opened and closed
chat through the keyboard. Switching back restored native light punch (`0010`) and right-stick chat. Four virtual
pads also kept their seats when one was disconnected and reconnected while native input was off.

These trials used a virtual SDL pad and injected keyboard input. A physical Windows controller with the reported
JoyToKey or reWASD profile, repeated dashes under internet conditions and a four-country rematch still need field tests.

The spectator preference was checked separately with live UDP hosts and an isolated launcher configuration.
Sixteen menu-host, rehost and direct-launch cases covered missing, enabled, disabled and invalid saved preferences,
hosts without a nickname, unlisted hosts and the explicit `--net-spectators` flag. Opening a fresh host from Esc
and returning to hosting after a match both picked up the latest saved preference.

## November 20 board timing integration (protocol 14)

The four-player build includes the board-speed default from upstream #425. All playing machines and spectators need protocol 14, even when two launchers display the same beta.3 version. Direct UDP tests in both directions refused a player on protocol 13 or 14 when the host used the other protocol, then admitted a matching player into the same seat.

| Local MVC run, one player per process | Shared confirmed frames | Frames matched by the late spectator |
| --- | ---: | ---: |
| Arcade perfect, Dreamcast teams | 1,488 | 1,449 |
| Every clock, adjacent teams | 1,490 | 1,450 |

Both matches used four processes, two frames of configured input delay and 50 ms simulated delay with 25 ms jitter and 5% packet loss on each sending process. Guests began with the opposite slowdown and gauge preferences. The offline input movie matched every shared frame in each run. These hashes measure simulation agreement. They do not measure physical input latency.

To repeat the first run, use `GAME=mvsc MACHINES=4 SLOWDOWN=0 MVC_TEAM_ORDER=0 MVC_GAUGE=0 FIRST_TO=1 WATCH=1 bash tools/check_online.sh 1500` with the built game and its ROM. Set `SLOWDOWN=1 MVC_TEAM_ORDER=1` for the second run. The fresh board-timing gameplay test also probes all four controller owners during simultaneous Duo in `tools/check_mvc_gauges.py`.

Version 14 disk saves combine the DMA progress added in upstream v13 with the partner-tag input history from preview v13. Seven original saves across MVC and Vampire Savior loaded. Each matched its equivalent v14 save for 90 simulation frames and produced a byte-identical re-save, including an MVC preview with both bench tag chords held.

## November 19 review checks (protocol 13)

These tests use local processes and scripted controller inputs. The uneven-link runs inject delay on each
outgoing link. The historical results below keep their original protocol and revision.

| Test | Observation |
| --- | --- |
| Both idle gauges, then ordinary attacks | From frames 4250–4499 both gauges stayed at **144/144** with fresh settings. With passive gain enabled they rose from **167/167 to 209/209**. The same attacks then brought the default gauges to **432/360** (144 units per bar). Both point owners successfully activated Duo. |
| Host choice | Four local processes tested a fighting-only host and a passive-gain host against three guests that requested the opposite setting. Both runs agreed through **584** confirmed frames and matched offline replay. |
| Sustained clock lead | In three 300-frame windows with jitter, host/guest pacing waits were **5/0** at a 1.5-frame host lead, **0/0** while level and **0/5** after the lead reversed. The budget is set once per 60 frames. A peer catching up cancels the unused part. |
| Four single-seat MVC processes | **1,195** common confirmed frames and identical offline replay. Host and guests used 25±8, 7±3, 38±13 and 55±20 ms outbound delay, respectively, with no loss and three frames of input delay. Pacing waits by machine were **15/0/0/0**. |
| Another game using the same rollback | Two *Marvel Super Heroes vs. Street Fighter* processes matched **895** confirmed frames and the offline replay, with 35±15 and 8±4 ms outbound delay. Host/guest pacing waits were **4/0**. |
| Spectator and host settings | Four MVC players accepted the host's adjacent-team order, fighting-only gauge and FT5. With 50±25 ms and 5% loss per sending process, all four matched **1,189** confirmed frames, the late spectator matched **1,133**, and the offline replay matched. |
| Protocol admission | A protocol-12 host refused protocol-13 guests and spectators, and vice versa. Both accepted a peer and spectator of their own version. The direct handshake and the lobby service check the protocol before allocation. |
| Reserve tag through hitstun | After a hit at frame 4424, the reserve held HP+HK at 4430. Native tag action started at 4455 and the point switched at 4469. Releasing at 4435 cancelled it. Releasing a successful request after entry did not tag the fighter back out. |
| Duo taunt ownership | Point Start plus reserve LP no longer taunts the reserve. The reserve's own neutral Start+LP does. Both halves of the chord come from the fighter's assigned controller. |
| Confirmed movies | A second recording whose first confirmation is frame 120 discards the previous session through frame 4000. Reconfirming a frame replaces its four input words. Saving and reloading keeps the final held frame. |
| Point-label rendering | The destination-row blend produced identical pixels and rectangles to the previous per-glyph renderer across **48** images and **8,257,536** compared pixels. |

Run the protocol, controller ownership and movie checks with the commands below. The game tests require your own ROM sets.

```sh
make check-netplay check-crossfever-reset check-movie check-online-service
python3 tools/check_mvc_gauges.py --rom rom/mvsc
python3 tools/check_mvc_bench_ownership.py --rom rom/mvsc
PEOPLE=1,1,1,1 GAME=mvsc MVC_TEAM_ORDER=1 MVC_CUSTOM=1 FIRST_TO=1 WATCH=1 tools/check_online.sh 1200
```

### Compatibility with upstream main

Compared upstream `ea9e101` with GitHub's merge of the first reviewed revision (`e00b538`, PR head `05fe6d7`). Both builds used the Release interpreter on macOS arm64.

| Existing behavior | Direct comparison |
| --- | --- |
| MVC and MSHvsSF arcade play | One paid player versus CPU and two paid players per game. All 21,600 per-frame hashes and all 24 screen captures were identical. The two-human games included native tags. |
| Classic MVC Tag team, Cross Fever off | Two more 5,400-frame runs matched on every game-state hash and on the fighter/team RAM traces. In the point-owner test the left team tagged at 4344 and 4734, the right at 4514 and 4914. Distinct inputs followed the active fighters both ways. The local POINT labels move from y=42 to y=68 to clear the native helper counters. |
| #414 upper, lower and hidden names | Four processes ran one match with different local display settings. All agreed on 1,509 confirmed frames. At frame 1500, hiding the upper labels changed 3,744 pixels. It changed zero pixels in the centered FT10 score or in the sampled board rows 14–79 and 110–164. |
| #416 Endless Battle foreground | X-Men vs. Street Fighter, The TV Studio, seed 23. All 2,000 picture hashes and all 2,000 non-picture state hashes matched main. A control with the pre-#416 renderer changed 325 frames, including 1,084 fighter/shadow-overlap pixels at frame 1375. |

**In-match menu and rollback (#415).** On Street Fighter Alpha 2, the host and guest picked Ken and Akuma for Ball battle, then Charlie and Chun-Li for Tag team. Both boards ran the selected pair. The host could return to Versus. A targeted save/restore test exposed a separate bug when the host started a mode on the same frame the guest picked a fighter. Replay saw the later pick and chose ID 6 where the original run had chosen ID 1. The correction orders commands by simulation frame, sender and position in the sender’s packet. Ball and Tag matched on all 350,945 saved bytes in each of eight replay cases. The command-order regression runs in `make check-netplay`.

For the paid versus comparison, run both executables separately from clean directories with the same ROM and [paid-versus movie](test-data/marvel-paid-versus.txt):

```sh
"$GAME_EXE" --rom "$ROM" --headless --frames 5400 --movie "$MOVIE" --settings settings.ini --eeprom board.eep --hash-log full.hash --shot-every 900:frame-
```

Compare the two `full.hash` files and the six PNGs. For MVC’s [four point tags](test-data/mvc-classic-point-tags.txt), add `--features free_play,tag_team=2` and check the controller handoffs above. On builds with the Slowdown selector, include `slowdown=1` in the feature list for both historical movies to reproduce their every-clock timing. `GAME=vampire_savior tools/check_online.sh 1500` also reproduced Tommy’s #415 transport test on the initial merge. Host, guest and offline replay agreed on 1,497 confirmed frames at 50±25 ms simulated delay and 5% loss.

[Four-player FT10 capture with long names](screenshots/mvsc/ft10-team-names.png). The whole-collection builds cover compilation. The comparisons above cover the named games and scenes.

## Reproducing the November 18 playtest

A community playtest of 1.6.0-beta.2 reached a four-player fight across four countries. Participants reported that
changing host affected playability. A separate connection attempt failed during the router handshake. A screenshot
showed the online labels covering the native helper count; another showed the host configuration with Cross Fever off.
The sessions lacked synchronized logs from all four machines. They establish neither the failing network leg nor a
Cross Fever-on ownership fault.

The public [HUD and meter report in issue #363](https://github.com/tmcleroy/cps2recomp/issues/363#issuecomment-6989388993)
has the original screenshot. To play four-owner Duo, select **Cross Fever → On**. Fresh settings earn meter through fighting. The host can switch **Cross Fever gauges** to **Fill by themselves**. Each player can hide names, streaks and ping locally or
turn down **Online HUD opacity**. These comparisons have static versions for readers who prefer stills.

| Comparison | Animated | Still |
| --- | --- | --- |
| Same game frame, previous HUD and two reduced-opacity settings | [HUD wipe](screenshots/mvsc/hud-opacity-comparison.gif) | [Matched capture](screenshots/mvsc/hud-opacity-comparison.png) |
| Three host-to-guest links during a 320 ms input stall | [UDP retry diagram](screenshots/mvsc/rollback-retries.gif) | [Retry counts](screenshots/mvsc/rollback-retries.png) |
| A physical input sampled during a wait and its next two accepted frames | [SDL input timeline](screenshots/mvsc/sampled-tap.gif) | [Press and release](screenshots/mvsc/sampled-tap.png) |
| Classic 2v2 with a point player's motion and a different player's tag chord | [Command ownership](screenshots/mvsc/tag-command-ownership.gif) | [Fighter inputs and meter](screenshots/mvsc/tag-command-ownership.png) |

Use a build from the same revision on **all machines**. The startup log gives the game, reported version, protocol and game signature. Protocol 11 introduced asymmetric rosters. Protocol 12 was the November 18 preview. Protocol 13 introduced the held-tag, taunt and roster changes. Protocol 14 requires the same board timing and rollback state layout for all players and spectators. The signature identifies game data for the handshake; record the source revision separately. Three-human joining by code
requires an updated introduction service. To isolate the service on a reachable route, join the host by address.
The production service is independently deployed.

With two builds of the same game/reported version on different protocols, run
`python3 tools/check_protocol_pair.py --program NEW_MVC --previous PREVIOUS_MVC --rom rom/mvsc`.
It hosts each build in turn, checks the incompatible guest is refused and completes a short match with a compatible guest.

With the MVC program built, run the controller and rematch checks using your own ROM directory or archive:

```sh
python3 tools/check_mvc_three_players.py --program build/mvsc/mvsc --rom rom/mvsc
python3 tools/check_mvc_tag_requests.py --program build/mvsc/mvsc --rom rom/mvsc
python3 tools/check_mvc_rematch.py --program build/mvsc/mvsc --rom rom/mvsc
```

`make check-gamepad-slots` attaches four SDL virtual gamepads, holds six buttons, removes two pads and reconnects them in reverse order. It checks the still-connected pads' slot numbers and button states, toggles **Keyboard only**, and reconnects a pad while native input is disabled. Identical pads without a unique serial or path use the first matching vacancy.

Use `python3 tools/check_host_delay.py --rom rom/mvsc` for six actual two-process
delay negotiations, including an explicit zero and conflicting launcher/game values.

The first MVC check plays from character select through both timed attacks and staggered expiry on both three-human
layouts. It reads the four actor records during the match. The second drives both teams' bench and point HP+HK,
and tests the original and Dreamcast-style taunt inputs. The third checks a second fight from cold boot, including
whether both teams can choose new fighters and whether those fighters remain human-controlled. These tests use
ordinary game inputs and read-only observations.

The frozen macOS arm64 interpreter (`SHA256=84f0419588de575cd390f4c112e061128cabd419c4df7d3debdc657392355a3e`)
passed eight cold-boot rematches: either KO winner, a win on time, a draw, the losing side pressing Start early,
both three-human layouts and an Arcade continue control. In Everyone reselects, each of the four picks was confirmed
by its assigned owner. Arcade continue reopened only the losing side. All eight reached a second human-versus-human fight.

The translated MVC executable (`SHA256=5cd9f32ea8a310dd00b374a368fe01dea22e0842eddfcd91f02337d3e76c4a76`)
passed both three-human layouts with 792 frames of simultaneous Duo in each. A 7,100-frame movie produced identical
state hashes in translated execution and its block-scheduled interpreter. A separate 4,500-frame direct UDP run
used four playing processes and a viewer on one Mac, four frames of delay, FT5, every custom match rule, and the four
outgoing link profiles below. The players agreed on 4,491 confirmed frames; the viewer matched 4,452 host frames.
The recorded controls reproduced the match offline. Other tests ran concurrently, so the timing logs from this
run should be used for diagnosing stalls, not estimating typical performance.

The translated build also passed the KO and time-win rematch checks. In a separate late-join run a spectator
joined at frame 352, matched 1,289 confirmed frames and left while the players continued. Local `make check`,
the reset and virtual-gamepad targets, the 12 native protocol groups and the repository audit passed.

All 45 Node service tests passed, including admission, invitations and spectators. The separate spectator check also passed. A local Worker
run with three single-controller copies confirmed 205 shared frames; four confirmed 198. All seven match rule
values were checked on every copy with deliberately different saved guest settings, including the selected
taunt and rematch choices. Display preferences remained per machine. The short Worker runs check admission and
rule negotiation; the separate gameplay tests exercise the mechanics.

To exercise custom choices in a longer direct-UDP replay test, use `MVC_CUSTOM=1` with a three- or four-human
layout. `FIRST_TO=1` requests FT5. `WATCH=1` adds a spectator whose input cannot reach the match.

For an uneven-route test, the profile for each machine is its *outgoing* simulated delay in ms, jitter in ms and
loss in percent, host first. These are synthetic values. They do not model a particular city or ISP.

```sh
for delay in 2 4; do
    NET_DELAY=$delay NET_PROFILES=15:4:0,38:10:1,82:25:2,52:15:1 \
        PEOPLE=1,1,1,1 GAME=mvsc tools/check_online.sh 750 || exit 1
done
```

A paired macOS arm64 interpreter run used executable
`SHA256=6bbf7877f01a4d12790d52ab65208c36f093517a639991ecdf4e777b4886d634` for both delays. Each row is
one run. Counted waits are polling ticks. The number of ticks per second depends on machine scheduling.

| Host delay | Common confirmed frames | Host rollbacks | Host re-simulated frames | Host prediction waits | Host pacing waits |
|---|---:|---:|---:|---:|---:|
| 2 frames | 745 | 184 | 664 | 151 | 19 |
| 4 frames | 746 | 57 | 248 | 130 | 17 |

Both captures replayed offline to matching hashes. Four frames traded about 34 ms of additional local input delay
for fewer corrections on these routes. Repeat with different hosts and hardware before selecting a group default.

For the next internet set, keep players/settings fixed and rotate the host. Record each host's three direct RTTs,
prediction waits, pacing waits, rollbacks, frame overruns and confirmed hashes through a match **and** rematch.
A joining player's on-screen ping includes an estimated route via the host. The direct-link log identifies the
measured hop. If a code lookup succeeds but no UDP reply arrives, try the same host over a reachable address or a
forwarded UDP port, then try a different host. This distinguishes a failed connection from rollback stalls after
play has started. Share logs with invite codes and network addresses removed.

### Protocol 11 after the upstream instant-replay merge

On macOS arm64, the MVC interpreter built from `a64294a` used protocol 11
(`SHA256=4dbb8488a3fd1b16b01b1f20e89e93fc37b690a249eaead0d4b214c8cf6dcb1b`). Running 1,200 frames with
four playing processes, a spectator and the unequal link profiles above gave 1,196 common confirmed player
frames and 1,058 matching viewer frames. Offline replay reproduced the common prefix. Both three-human
layouts sustained 792 frames of simultaneous Duo. Arcade continue and Everyone reselects each reached
a second human fight after a left-side KO. The 12 native protocol groups, 45 Worker tests and separate
spectator-service check passed. These runs used one computer.

| MVC comparison | Animation | Still |
| --- | --- | --- |
| One controller per machine across both four-person orders and both three-person layouts | [Follow each controller](screenshots/mvsc/mvc-roster-routing.gif) | [Owner assignments](screenshots/mvsc/mvc-roster-routing.png) |
| Who gets a new pick after the same KO and early Start | [Rematch flow](screenshots/mvsc/rematch-selection.gif) | [Four confirmation frames](screenshots/mvsc/rematch-selection.png) |

### Four-player team order

The macOS arm64 interpreter used for the ownership regression had
`SHA256=42b1ad341bbfd62376a911feef7e0aa7611435f7d2b19a6aa3f42581b41fc604`.
`check_mvc_team_order.py` cold-boots MVC with four simulated controllers and ordinary inputs in each layout.

| Check | Result |
| --- | --- |
| Select each fighter | In both runs the left team confirmed at frames 1,400 and 1,452, and the right team at 1,424 and 1,476. Switching the host rule swapped which people made the middle two picks. |
| Attack and tag independently | Both orders exercised all six attack buttons on each fighter during 1,021 frames of simultaneous Duo, both expiries, and four isolated normal tags. The previous build failed the paired-order pick at frame 1,448. |
| Helper selection and calls | Each point owner could confirm their side's helper. A reserve button left the selector waiting. In combat, left and right point MP+MK spent one call on their own team. |
| Alternate online order with default guest settings | Four local processes matched through 1,495 common confirmed frames. The viewer matched 1,387 frames. Replaying the recorded controls offline reproduced the confirmed hashes. |
| Mixed local seats | Four people on three processes in a 1,2,1 arrangement matched through 437 frames and the offline replay. |
| Admission, set results and spectators | All 13 native protocol groups, 46 service tests and the separate spectator service test passed. Alternate rooms refused a previous protocol-11 player and viewer before accepting replacements. In default rooms, matching interpreter runs admitted the previous protocol-11 client in both host directions. |

Use the same build and CPU mode on all playing machines. One mixed run with an earlier translated executable on its
default CPU and the updated interpreter desynchronized at frame 60. Forcing interpreter execution on those binaries
gave matching common frames in both default-room host directions. The game signature alone does not guarantee
equivalence between every CPU configuration. Physical controllers and a four-machine internet set remain to be tested.

```sh
python3 tools/check_mvc_team_order.py --program build/mvsc/mvsc --rom rom/mvsc
MVC_TEAM_ORDER=1 PEOPLE=1,1,1,1 GAME=mvsc FIRST_TO=1 MVC_CUSTOM=1 WATCH=1 tools/check_online.sh 1500
```

### Choosing a host for four distant players

With four single-controller machines, each guest sends input to the host, which forwards it to the other guests.
A guest can have a good connection to the host while waiting on inputs from another guest. [Example of the two links](screenshots/mvsc/mvc-host-routes.png). The host's three
direct RTT readings measure individual round trips. The guest-to-guest number shown by the game is an estimate.

For a group in London, Canada, California and Venezuela, try each willing player as host. Keep the game build,
controller assignments by fighter where possible, gameplay sequence, and input delay consistent. Use the same
two tests each time:

1. In classic 2 vs 2, tag in both directions and confirm that point control follows the tag. Record any
   quarter-circle and HP+HK from different teammates that activates an unintended team move.
2. Enable Cross Fever. Have both teams enter QCB + HP+HK, have each of the four owners give distinguishable
   commands during the overlap, and repeat across expiry and a rematch. Record each machine's confirmed hashes.

Log direct RTT samples, bursts of missing packets where measurable, prediction waits, pacing waits and frame overruns.
Compare intervals of equal length early and late in the set. Ask all four players whether taps register and whether
the chosen delay feels playable. Repeat the best candidates. The two input-delay settings in the
[measured comparison](screenshots/mvsc/mvc-host-delay.png) came from unequal simulated links on one computer.

**Proposed lobby improvement.** Probe the reachable candidates before a match and show how each performs on its
worst guest-to-guest route. A timed guest → host → second guest → host → first guest echo would measure the full
forwarded round trip using one clock. Compare the 95th percentile and loss bursts for each candidate. Also
show direct-link readings so a player can identify a weak host connection. This pre-match probe is a proposal.
An idle versus busy uplink comparison tests whether traffic at home adds delay.

Measurements of U.S. fiber show that route length alone can underpredict delay. A console matchmaking study used
latencies between actual machines to improve predictions. CAKE's residential gateway measurements motivate the
busy-uplink control. These sources concern routing and queuing. A four-country Marvel match and rematch on this
build still needs live measurements. [Bozkurt et al., arXiv](https://arxiv.org/abs/1811.10737)
· [Agarwal and Lorch, SIGCOMM](https://www.microsoft.com/en-us/research/publication/matchmaking-for-online-games-and-other-latency-sensitive-p2p-systems/)
· [Høiland-Jørgensen et al., arXiv](https://arxiv.org/abs/1804.07617).

Run `python3 tools/online_windows.py host.err guest1.err guest2.err guest3.err` to turn each machine's
cumulative reports into CSV intervals. Each row gives the frames advanced, rollbacks, re-simulated frames,
frame overruns and prediction/pacing wait ticks since that machine's previous report. New connections start
a fresh baseline. In a partial log, the first sample provides the baseline for the next. Poll counts depend on
machine scheduling, and clocks and frame ranges can differ between machines.

## Four-player input and timing checks, November 18, 2026

**What can disappear before a packet is sent.** An SDL virtual controller pressed LP during a prediction
wait and released it by the next accepted game frame. The old frontend sampled both states, then submitted
two neutral game frames. Holding LP through the wait worked. The playing window now keeps up to four distinct
whole controller samples for at most 100 ms and submits them in order, one per accepted frame. A sample from
a frame that runs immediately incurs no additional frame of delay. Overflow and expiry resume with the current
held state. A menu, a new session or a disconnected seat clears the corresponding queued input. The test covers
two local controllers, separate presses, simultaneous chords, releases and fast direction reversals. Both the
polling interval and the bounds matter when interpreting a player's report. In a second, two-process test the
actual game window received a virtual SDL LP press and release while the guest was briefly paused.
Bypassing the queue produced zero LP frames in both confirmed movies. With the queue enabled both recorded
LP (`0010`) on frame 92 and release (`0000`) on 93.

### Which frame receives a stalled input?

The physical controller, the SDL polling loop and the simulation advance on different clocks. On a prediction wait,
the local game frame stays at the same number. **Retain short taps** keeps each observed controller state in order.
Each new simulation frame consumes at most one queued state, scheduling it at that frame plus the host's input delay.
**Current state** consults what is held as that frame advances. Either mode can miss a tap entirely between polls.
Each machine chooses its policy locally under **Online → Input during network waits**, including in the match menu.

In a two-process window test of the combined protocol-12 revision `9851887`, the local game stalled at frame 90.
A virtual SDL controller pressed LP and released it while the guest was paused. Both trials used two frames of host
input delay. The host window was instrumented to inject controller states and observe the live and replayed frames.
The instrumented host had
`SHA256=8268827cdabb6f06d4b22a5f3b03be7c2e63a990e9841162df0b31a7d4b2aaee`.
The other process used the unmodified MVC interpreter (`SHA256=2a966880ecb0e3b8d91d675e1c19b732509fec9a643b1e9ebaaa595a314ee819`).

| Local setting | SDL observations during wait | Accepted local inputs, frames 90 / 91 | Confirmed input on both peers, frames 92 / 93 |
| --- | --- | --- | --- |
| Retain short taps | LP, release 19 ms later | LP / neutral | `0010` / `0000` |
| Current state | LP, release 17 ms later | neutral / neutral | `0000` / `0000` |

Each trial matched across 224 common confirmed frames. The two release times reflect the actual SDL polls.
Retaining a tap can move the action later in real time, including past a change in fighter state or facing.
When rollback resimulates, it reads the local input already assigned to that frame. [Timing diagram](screenshots/mvsc/input-history-timeline.png) · [Animated trace](screenshots/mvsc/input-history-timeline.gif)
· [Recorded controller words](test-data/mvc-stalled-input.csv).

In the zero-delay test on this same build, the controller held Left and tapped LP twice.
The confirmed movie recorded Left+LP, Left, Left+LP and Left on frames 86–89. During a rollback of frame 86
the remote input changed from `0251` to `0040` and the local word remained `0012`. The Current state trial
kept Left with zero LP frames. Each pair agreed through 224 common confirmed frames. Separately, the portable
SDL test distinguishes a real LP+MP overlap from a release between the two buttons. [Double tap and chord traces](screenshots/mvsc/sampled-chords.png).

The bounded queue holds four changes for the whole local poll, including both pads if two people share a machine.
The oldest pending change reaching 100 ms, or a fifth distinct change while full, clears the backlog. The next
advancing frame takes the current held state. The age starts with the sampled change, so a fresh tap near the end
of a longer stall can still qualify. A partial command can reach the game before its remaining samples expire.
No test here measures the player's intention, a physical pad's polling interval, or the cause of a reported missed Super.

To reproduce the portable controller cases, run `tools/check_netplay.sh` and look for the SDL taps test.
For a field A/B, keep the host, teams, host input delay and move fixed. Switch one machine's wait-input choice.
Record its option, which direction the fighter was facing, whether it could act, and the two confirmed input movies.

On the same combined build, `tools/check_mvc_team_order.py` exercised all 24 fighter/button assignments, independent inputs during 1,021 frames of simultaneous Duo, the expiry of each timer and four ordinary tags in **each** four-player order.
The twelve ordinary-input movies in `tools/check_mvc_tag_motion.py` exercised both orders. In classic mode the point's
QCB and the reserve's HP+HK requested ordinary tags and preserved the teams' three and two earned bars. The previous
paired-order branch activated both Duos and spent both gauges on the same inputs. Each point owner's full QCB+HP+HK
still triggered Duo. The left point's full QCF+HP+HK spent two bars on the team Hyper. Cross Fever also passed each
tag-permission setting in each order.

With adjacent teams, four loopback processes using distinct simulated links agreed on 1,193 confirmed frames,
the spectator agreed on 1,124, and replaying offline reproduced the common hashes. The 16 native protocol groups,
49 Worker tests, spectator service, reset, virtual controller and build checks passed. These processes shared a Mac.

**Why the host paused.** In a controlled four-peer test, a stale clock estimate previously requested 16 pauses,
eight after the peers had caught up. The same test now gives eight pauses, spaced at least eight frames apart,
and zero after catch-up. In a separate fixture, 320 unchanged polls in 320 ms sent 960 control datagrams over
three host links. With a 32 ms quiet retry they sent 30. New input, newly received streams and advancing frames
still send immediately. The simulation retains its eight-frame prediction limit. These are counts for these
specific fixtures, not a percentage reduction in normal traffic.

**Classic Tag team.** Run `python3 tools/check_mvc_tag_motion.py --program build/mvsc/mvsc --rom rom/mvsc`.
The old build combined the point player's QCB and the reserve player's HP+HK on each team, activated both
Duos, and depleted three bars on one team and two on the other. All six ordinary-input cases now pass.
The reserve's chord can request a normal tag. A point owner can still enter their complete team Hyper or
Duo. Cross Fever's two tag-permission settings also pass. Five offline virtual-pad removal cases covered
active and unused seats. Four online playing processes continued from frame 984 through 1008 while the
host's virtual controller disconnected and reclaimed its slot.

**Visibility.** The HUD images above share board frame 1680 (hash `de76b4de`). Outside the overlay regions every
pixel matches. Soft uses backplate alpha 112/255 and text alpha 232/255. Faint uses 64/255 and 190/255.
High contrast uses 220/255 and 255/255. Changing a local preference left four peers' game hashes in agreement.
The team names sit below MVC's helper counters. The set score uses a small center strip on the native score row.

### Four processes under unequal simulated links

Two 18,000-frame runs on one arm64 Mac used the same automated inputs, three frames of input delay, and the
four outgoing delay:jitter:loss profiles below. The hash count is the prefix confirmed by all four processes.
In each run replaying the host's confirmed inputs offline reproduced that prefix.

| Run | Common confirmed frames | Host rollback count | Host prediction / pacing wait ticks | Host frame overruns | Recorded input rows / bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| Previous PR | 17,994 | 1,697 | 208 / 73 | 203 | 17,998 / 456,922 |
| Protocol 12 pacing and recording changes | 17,996 | 2,902 | 1,320 / 207 | 1,104 | 7,345 / 186,507 |

The second run had more frame overruns on **all four** processes. Other builds and tests shared the machine,
and the two runs had different scheduling. These results establish deterministic replay over this interval.
They do not establish a latency, throughput or long-session improvement on the internet. The recording is
smaller because it stores changed inputs and a final duration marker when the last input is held. Recording
only runs when requested. The 18,000-frame automated runs bypass interactive SDL sampling.

Translated executable SHA-256, previous `SHA256=5cd9f32ea8a310dd00b374a368fe01dea22e0842eddfcd91f02337d3e76c4a76`,
pacing build `SHA256=db5f36a3774d24f05d3c82103663f9fae38b0fa879aa925d628f675c47e19c53`. Reproduce the
profile with a built MVC program and a local ROM set:

```sh
GAME=mvsc PEOPLE=1,1,1,1 MVC_CUSTOM=1 NET_DELAY=3 \
  NET_PROFILES=15:4:0,38:10:1,82:25:2,52:15:1 tools/check_online.sh 18000
python3 tools/online_windows.py work/mvsc/check/online-{host,join1,join2,join3}.err
```

A shorter follow-up ran the previous and final executables sequentially with the same profile, 3,600 frames each.
The common confirmed prefixes were 3,597 and 3,594 frames respectively, and each replayed identically offline.
On the host, waited ticks went from 315 to 189 and frame overruns went from 144 to 33. All three guests also
had fewer overruns in this pair. The opposite direction in the longer run is a reason to collect repeated
field measurements before making a general performance claim.
The previous and final programs also matched all 4,640 state hashes offline with optional game features off.
Translated executable for that paired follow-up `SHA256=201798cf1ca15986dea11aa5aec7f11142c0bc49225701d0b0d10d1fc1ee14e5`.

After integrating upstream instant replay (`0acd4b7`), four playing processes agreed through 1,194 frames
of a fresh 1,200-frame run and reproduced them offline. Both the interpreter and translated programs rebuilt.
Merged translated executable `SHA256=1238af237d4e6f3756eae7a5f7a641a0aa070dc8777bfc08ec86d08fb78ab4d4`.

The first neutral sample, an isolated P4 chord and release, a trailing hold, and a shorter rehost were also checked by saving and reloading the input movie.

The port keeps its own host-forwarded UDP rollback. GGPO's comparison of local and remote frame advantage
informed the pacing test. Its UDP implementation is also a reference for retrying during quiet periods.
The 32 ms retry above is specific to this code. For the next internet set, rotate the host with the same
four players and capture per-link RTT and wait counts by interval. Repeat under an idle and a busy uplink to
distinguish queuing delay at the access link from CPU overruns and missed local input.
[GGPO time synchronization](https://github.com/pond3r/ggpo/blob/7ddadef8546a/src/lib/ggpo/timesync.cpp)
· [GGPO UDP](https://github.com/pond3r/ggpo/blob/7ddadef8546a/src/lib/ggpo/network/udp_proto.cpp)
· [CAKE access-link measurements (arXiv)](https://arxiv.org/abs/1804.07617).

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

## Targeted verification, November 14, 2026

These checks cover the networking fixes through `b394926`, including `6dff752`. They do not rerun or replace
`471f7f7`'s gameplay results below.

- **Set-result reordering:** the production C++ regression previously failed when `ready=true` was followed by
  a delayed `ready=false` for the same target and score. The later host goodbye then looked like a departure.
  Readiness now stays established for that exact result. Duplicate unready packets, changed scores, a different
  target, final goodbye and a new in-memory session are covered; all **10 protocol groups pass**. This adds no
  wire-session nonce and does not establish protection against packets replayed across sessions.
- **Invitation RPC interleaving:** five new regressions fail on the previous Worker and pass with the fix.
  Deferred external Room calls expose ticket rollback during rehost, loss of a concurrent admission during poll,
  same-millisecond replacement overwrite, expired-record resurrection and a decline overwritten by an away reply.
  The host stores its new generation before calling Room; a reply merges only into fresh, live, matching state.
  The generation is internal storage metadata, not a new HTTP or UDP field. Full Worker suite: **39/39 pass**;
  the separate spectator service suite also passes. These substitutes test controlled external-await boundaries,
  not Cloudflare's complete scheduler or a deployed service.

Reproduction uses the two ROM-free commands at the top of this document and
`node online/test-spectators.mjs`. On this arm64 Mac, the C++ check used Apple Clang 21 with
`CXXFLAGS='-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk'`; the installed SDL library still
warns that it targets a newer SDK. Whitespace and repository audit checks pass.

A fully deleted or expired service record can still restart its ticket counter; the stored generation protects
late invitation work, not old client removal tickets across record deletion. Four-machine internet/rematch,
physical-controller, hosted CI and production Worker deployment checks remain outstanding.

## Local verification, November 13, 2026

The merge with upstream `eea67d1` was checked on an arm64 Mac with the MVC interpreter build, Apple Clang 21,
SDL 3.4.20 and Node 25.9.0. The combined wire protocol is 10; new save states use version 12.

- The MVC build and `make check` passed, including the repository audit, shared-source check, first-to scoring,
  example build and launcher smoke checks.
- 34 Worker tests, the service's separate spectator suite and 10 production C++ protocol groups passed. Added
  cases cover same-code rehost ticket retention, protocol rejection before reservation, all-peer set agreement,
  reliable mode-event delivery, four-person mode restrictions and four-stream/read-only spectators.
- The Cross Fever reset/state suite passed. It also checks the marked MVC parts of both version-11 layouts,
  preserves the next section when legacy parts are absent, and rejects missing required version-12 parts. These
  focused section tests do not promise that arbitrary old whole-game saves load across different builds.
- All six layouts passed 3,000-frame runs with `WATCH=1 FIRST_TO=1`, 50 ms simulated delay, 25 ms jitter and 5%
  packet loss. Assignments, names, chat, host-selected FT5 and offline replay passed. The four-person runs enabled
  Cross Fever. Counts below are common confirmed hashes, not total frames requested:

| People per machine | Playing machines' matching frames | Viewer's matching host frames |
|---|---:|---:|
| `1,1` | 2997 | 2405 |
| `2,2` | 2998 | 2585 |
| `2,1,1` | 2987 | 2961 |
| `1,2,1` | 2990 | 2972 |
| `1,1,2` | 2989 | 2972 |
| `1,1,1,1` | 2988 | 2969 |

- A separate 6,700-frame player run used the production Worker through its loopback-only test service. The viewer
  joined by code at confirmed frame 4,576 (first hash 4,577), beyond the 4,096-frame history window, and matched
  1,124 host hashes. Its synthetic controls and exit did not affect the players; their hashes matched too.
- A 120-frame headless `--watch-writes FF0000-FF0001` run emitted memory-write diagnostics and exited normally,
  instead of attempting to spectate. Shell syntax and whitespace checks passed.

These runs establish local simulation, negotiation and replay behavior, not completion of a whole FT5/FT10 set.
All-peer set completion is covered by the production protocol test with synthetic scores; whole-set gameplay,
physical controllers, real-router/NAT and four-machine internet tests remain separate. The Worker was not deployed.
Windows/Linux runtime checks and hosted CI results are not implied by these macOS results. Build warnings included
existing deprecated codecvt use and the installed SDL library targeting a newer macOS SDK.

## Local verification, November 11, 2026

These results belong to commit `4b578bf`, before the later upstream merge. They do not validate changes added by
that merge; the expanded set/spectator checks above must be rerun on the merged build.

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
