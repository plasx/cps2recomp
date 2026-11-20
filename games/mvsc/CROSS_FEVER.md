# What Dreamcast Cross Fever changes

Cross Fever is the four-controller match format. Variable Cross (Duo Team Attack) is the timed move entered with
QCB + HP+HK. The arcade already has the move, its meter debit and its exit logic. In Dreamcast Cross Fever, both
teammates can give different commands for the entire time they are on screen together.

| Behavior | Arcade MVC / Tag team | Dreamcast US Cross Fever | Recomp Cross Fever |
|---|---|---|---|
| Normal tag | HP+HK changes the point fighter. In Tag team, control follows the point and either teammate can request a tag. | The point owner requests the tag. Each person stays with their fighter. | Point by default. With Either selected, holding a fresh bench HP+HK requests one tag as soon as the arcade allows it. Releasing cancels the request. |
| During Duo | One panel per side drives both fighters. | Four fighter records read their respective controllers, including simultaneous Duos. | Same four-owner design. Three-human versus assigns both records on the solo side to one person. |
| Meter and clock | Meter gained through play. Duo decrements on each eligible countdown step. | Adds a gauge unit every seventh game step outside that team's Duo. Its Duo timer decrements on odd eligible steps. | Earned meter and Dreamcast Duo length by default. The host can enable passive meter or choose the arcade countdown independently. |
| Starting order, KO and Variable Counter | The board determines which fighter is on point. | At a tag, KO replacement, Variable Counter landing or Duo exit, the controller follows the originally chosen fighter. | The arcade makes the fighter decision. The port preserves the owner, including when the second pick opens. |
| Taunt | Start. | Held Start plus the configured attack alone. Default LP. Start alone pauses. | Held Start by default. The Dreamcast chord is selectable. Online matches continue to simulate while the local menu is open. |
| Selection and rematches | Arcade selection and challenger/continue flow. | Four controllers required. All choose; new fighters can be selected before each match. | Arcade continue by default. **Everyone reselects** opens both teams’ native cursors after a human versus result. Three-human rooms are an extension. |
| Controller disconnect | The board uses wired panels. | The console checks the controllers and pauses play on removal. | **Pause on controller disconnect** opens the local menu if an active player's pad disconnects during an offline fight. Reconnect and close the menu to resume. Online play continues when a pad drops. |

## Where Tag team fits

Tommy McLeroy wrote the local Tag team routing for X-Men vs. Street Fighter, then included it in the MVC
port. It switches the shared arcade panel to the person whose fighter is on point. Morphus56K added the
per-fighter Cross Fever input path and extended the project's UDP rollback to four participating machines.
Tommy's later **Tag team battle** is a separate alternative mode for two online players in supported games
such as Street Fighter Alpha 2. MVC uses the arcade program's own tag combat; its game module registers no
alternative team battle. Choose the MVC Tag team layout before hosting. A reserve player's HP+HK requests a
normal tag through the game's tag check. Completing a team Hyper or Duo requires the full command from the point owner.
In a four-player room, the default pairs P1/P3 against P2/P4. The host can choose P1/P2 against P3/P4.
In classic Duo the point controls both fighters. Cross Fever lets each split-team owner steer their own.
[Original Tag team](https://github.com/tmcleroy/cps2recomp/commit/ad2fd20)
· [MVC port](https://github.com/tmcleroy/cps2recomp/commit/1dd905b)
· [Cross Fever](https://github.com/tmcleroy/cps2recomp/commit/8b792e7)
· [Four machines](https://github.com/tmcleroy/cps2recomp/commit/d3c17bd)
· [Two-person mode switching](https://github.com/tmcleroy/cps2recomp/pull/349).

## What the donor shows

The original US retail SH-4 executable used for this comparison is 4,930,596 bytes, loaded at `8C010000`
(`SHA256=785c8691e7c2a0fcbeeef6c478db960fef7c354cd9eb9bb6d6fd7b5b92526db5`).
The code and observations below identify the revision and distinguish a game step from a displayed frame.
The CPS2 comparison uses the European `980123` program.

- **Ownership and timing.** In a three-bar side-by-side run, both versions took 36 displayed frames from debit to
  Duo entrance. The initial counter was 576. From entrance to expiry the Dreamcast took 1,021 frames and the port
  took 1,023 in the measured run. That is close timing, with a measurable difference. At expiry the game checks
  the fighters' actions before deciding who leaves. A single capture cannot establish an unconditional swap rule.
- **Ordinary four-controller play.** One 27,764-vblank Dreamcast run selected the mode through its menu, tagged, and
  activated both teams' Duos with physical-port inputs supplied by a script. The left timer reached 576 at vblank
  20,480 and the right at 20,517. The two ownership selectors flipped independently at 21,537 and 21,539. At 23,543
  both were zero again going into the next selection. This trace covers the transition, through the first round.
- **Next match.** The console's result code writes 120 to its countdown at `8C054652`, then clears both selectors
  at `8C05469A` and `8C05469C`. Starting the alternate pick toggles the left and right selectors separately. A
  regression in the port retained the previous starting order across a warm board boot. The boot now clears both. On arcade continue, a winner can proceed against the CPU if the loser leaves. With **Everyone reselects**, the game records the result before both sides choose again. Four-person versus still assigns each pick to its original human seat.
- **Taunt.** At `8C0326F2` the console looks up an attack binding. At `8C032700..8C03270C` it compares one attack
  bit against the *whole* held fighter input word. LP, MP, HP, LK, MK and HK have masks 0x10, 0x20, 0x40, 0x100,
  0x200 and 0x400. Holding an extra attack or direction fails that comparison. The equivalent held word is at
  fighter +0xCE on SH-4 and +0xD0 on CPS2. In this port the host can choose the binding for the room.

The prior investigation catalogued 50 apparent Cross Fever flag references. Fourteen were reached in the sampled
fights, and one static match was data. Other sites include selection, results, vibration, controller disconnect and presentation. A mode-flag search
cannot identify a general character balance change that applies to *all* modes. [Existing traces and checks](PROGRESS.md).

## Console features and reported quality

Capcom documented four-pad setup, per-move button configuration, reduced flashing, screen adjustment, VMU saves
and optional vibration. The manual also describes handicap and stage selection in two-player Versus and a training
menu with separate health, helper and Hyper-gauge settings. These are concrete control and presentation differences. The port has its own control maps,
display options and saves. The console's selection and results screens, exact per-controller vibration and VMU integration are outside the current combat implementation. [Capcom's US Dreamcast manual, pp. 2–14](https://www.video-games-museum.com/en/manual/Dreamcast/25027_us-Marvel-vs-Capcom-Clash-of-Super-Heroes.pdf).

**One sprite comparison.** The CPS2 Chun-Li idle sequence starting at 0x139874 has 18 records and references
92 distinct 16×16 tiles. All 23,552 palette indices matched tile data in the Dreamcast fighter pack
`PL0BSP.GFL` after converting the CPS2 pen ordering. For Wolverine's idle at 0x113B38, 467 of 472 referenced
tiles mapped into `PL04SP.GFL`. All 119,552 compared pixels matched. Five tiles were outside that packing map.
The existing graphics-page alignment table supplied the candidate pages, then the low eight tile-index bits
selected each tile on the page. This establishes that the same tile detail exists in both sets. It leaves the
Dreamcast's live animation sequence, palette RGB, output scaling and frame pacing unmeasured.

**One combat comparison.** We cold-booted US Dreamcast Cross Fever, the European arcade program on the recomp's
two-player settings, and the same arcade program with four-person Cross Fever. Ryu stood 62 world units from
Spider-Man and pressed LP+LK once. The other three controllers stayed neutral during the attack. In all three runs
Spider-Man took one hit, his life went from 144 to 129, and Ryu returned to standing after 55 simulation steps.
On the Dreamcast the target reacted nine steps after the sampled command; on both arcade profiles it took eight.
The reaction ended 44 displayed frames after the command on all three. The two arcade traces agreed throughout
the 86-frame sample. Delaying the arcade chord by zero through five displayed frames in separate cold runs yielded
15 or 17 damage and contact after seven to nine steps. The Dreamcast observation falls within that range. Spider-Man's
vertical position differed by 0.875 units between platforms, and a display sample can span two game steps.
This case gives us no damage correction to port.

To replay the arcade comparison at its recorded every-clock timing, use the [two-player movie](movies/ryu_contact_arcade.txt) with `--features slowdown=1,free_play`
and the [four-player movie](movies/ryu_contact_cross_fever.txt) with `--features slowdown=1,free_play,tag_team=2,cross_fever`.
Use a fresh settings file and EEPROM for each. With `--frames 4640 --trace FF3000-FF449F:trace`, compare the
attacker and victim records at FF3000 and FF3400, in particular +0xD0 (held), +0x270 (life), +0xB0 (action)
and +0x06 (state). The attack is on movie frame 4500. On Dreamcast the corresponding life word is +0x268.

No general advantage in input latency, hit boxes or damage follows from these results. To port a
suspected character fix, record the two revisions with the same fighter/form, opponent, facing, speed, starting meter
and ordinary button sequence. Compare the first differing action, game step, contact, damage and recovery. A
localized discrepancy can then be tested separately with Cross Fever off and on. A stock two-player control keeps
ordinary arcade balance changes distinct from four-player ownership fixes.
