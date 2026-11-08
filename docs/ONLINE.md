# Online play

Two players, each on their own machine, play a match with **rollback** netcode; in the tag-team games, four players on
up to four machines. Any mix of macOS, Linux, the Steam Deck and Windows can play each other.

![The launcher's Online page: open matches, joining with a code, and hosting](screenshots/launcher-online.png)

## Starting a match

Each player needs the game and its ROM set. The launcher's **Online** page (in the sidebar) has it all:

- **Open matches**: the matches other players are hosting in the lobby, in every game, each with **Join**. A match in a
  game whose ROM set you haven't added is greyed, with **Add…** to add it.
- **Join with a code**: type the code a host gave you and press **Join**. The launcher starts the game the match is in.
- **Host a match**: choose a game and press **Host**. The game shows the match's code to give the other player (the
  other three, in a match of four: they all join with it). If you've set **Your name**, the match is also listed in
  the lobby under it until it's full; without a name, only those you give the code to can join.
- **Waiting room**: the other players whose Online page is open now, under their names. Choose a game and press
  **Invite** next to someone: your game starts and hosts a match, and they see the invitation on their Online page,
  naming you and the game, with **Accept** and **Decline** (and **Add…** if they haven't that game's ROM set). Accept
  starts their game and joins yours; your game's wait says if they declined. You're in the waiting room while your
  Online page is open and you've set **Your name**, unless you switch off **Let others invite me**. An invitation never
  interrupts a game: it waits on the Online page. **Block** (on a player or an invitation) hides the two of you from
  each other; **Unblock all** undoes it. Nobody's address is shown, only names.
- **Host from the game**: playing a game on your own, open its menu (Esc), go to **Online** and choose **Host a
  match**. Your game stays exactly your own while you wait: modes, training, cheats, the menu, save states, pause. The
  bottom of the screen says HOSTING with the match's code, and with a name set on the launcher's Online page the match
  is in the lobby too. When someone joins, the game starts over from the title for the match, with the match's
  settings. After the match you're back to your own settings and still hosting with the same code (unless you left
  the match yourself). The menu's row, now **Stop hosting**, stops it.
- **Arcade while waiting** (a switch under Host a match): play the game on your own while you wait, as at the arcade.
  The match's code and "WAITING FOR A CHALLENGER" show at the bottom of the screen. When someone joins, they come
  into your game as it is and press Start to challenge you. Esc asks whether to stop hosting.

Both players need the **same version** of CPS-2 Recomp; the launcher shows its version at the top of the sidebar.
A match on another version is greyed in Open matches, and a code for one says so. Your ROM sets can be different
regions of the same game (`mvscj` against `mvsc`, say): the match is played as the host's region. That is best
effort: if the game reads a part of its program where the two sets still differ, the match stops as a desync does
(the same set is surest).

Each game's own page has the same under Play, in its **Online** card. Nothing needs setting up on either router. The
host is player 1. The game's window says it is waiting until the other player comes, or how many more a match of four
waits for (Esc gives up). From a terminal:

```sh
cps2recomp-launcher --play sfa3 -- --host                  # the host: shows a code, waits for the other player
cps2recomp-launcher --play sfa3 -- --host --net-arcade     # ... playing the game meanwhile
cps2recomp-launcher --play sfa3 -- --join CODE             # the other player: the host's code (or address)
cps2recomp-launcher --play mvsc -- --host --features tag_team=2  # a match of four: three more join with the code
cps2recomp-launcher --play mvsc -- --join CODE --net-people 2    # ... two of them at this machine
```

## How it connects

- **How the code works**: a small service of ours introduces the two games, telling each where the other can be
  reached, and they then connect straight to each other through their routers; none of the match goes through it
  ([online/README.md](../online/README.md)). A code lasts 30 minutes and is good for one match: everyone in a match
  of four joins with the same code, until the match is full.
- **Joining by address** works too, without the service: on a home network, the host's own address (the Online card
  lists it); over the internet, the host's public address with UDP port 7845 forwarded to it on the router (`--host
  7900` and `--join ADDRESS:7900` for another port); or a VPN such as [Tailscale](https://tailscale.com). That is
  also the way for the rare two networks a code can't connect (some mobile networks). In a match of four, each
  joining machine goes to the host's address the same way.

## In a match

- **Each player presses Start to come in**, as at the arcade: the first to press plays the computer until the other
  presses theirs ("Here comes a new challenger").
- **Each player plays with their own player 1 controls**, and hears and sees the game as their own settings have it
  (the Display page, the shader, the custom soundtrack). There's no zoom online: both see the whole picture, as the
  board shows it. The match takes the host's board settings
  (difficulty, timer, rounds, the Marvel games' game speed and turbo...) and the host's features that change play (free
  play, arcade timing, widescreen, hidden characters...), for the match only: the other player's own are left as they
  are. The host chooses them before hosting (the launcher's pages, or the in-game menu before choosing Host); during
  the match the in-game menu shows them as the match has them and doesn't change them.
- **Four players** (the tag-team games: X-Men vs. Street Fighter, Marvel Super Heroes vs. Street Fighter, Marvel vs.
  Capcom): with **Tag team** at **2 vs 2** on the host (Alternative modes), a match is four people, on two, three or
  four machines: **People at this machine** (the Online page, under In a match) says how many of the four play at each,
  **1** (each on their own machine: four machines) or **2**, with that machine's player 1 and player 2 controls (two
  machines of two, or one of two and two of one). Everyone joins with the host's one code (or address), and the match
  begins when all four are there; the wait says how many more it needs. The host's are players 1 (and 3, with two),
  and the others' the rest in the order they came, two at a machine taking a whole team where one is free (players 2
  and 4). Each person chooses and plays their own character, as 2 vs 2 does on one machine. The host's Tag team is
  the match's, and so are the modes made for it (Marvel vs. Capcom's Cross Fever). With **Co-op vs. computer** the two
  players are one team against the computer, one at each machine. The rest is as for two: rollback, the checks, the
  names (each of a side's people under its life bar).
- **Rollback**: each machine runs the game with every player's controls, its own players' held back 2 frames (as a
  board's game might be; `--net-delay N` on the host, 0 to 8), and the others' guessed until they come. When a guess
  was wrong, the game goes back to that frame and runs on again with the real ones, unseen and unheard, in the time of
  one frame. Up to 8 frames are guessed; past that the game waits for the others. With more than two machines the host
  is between them: each machine sends the host its controls, and the host sends each the others' with its own.
- **The games are checked against each other**: every second each machine sends a fingerprint of the game (the
  joining machines to the host, the host to each); if they ever differ, the match stops and says so (a desync, which
  should not happen).
- **The ping** is said at the bottom of the screen all through a match ("PING 42 ms": to the farthest player, with
  more than two machines), and the input delay too if you like ("DELAY 2"): each is a switch, **Ping** (on) and
  **Input delay**, on the Online page under In a match (and on the Features page).
  "WAITING FOR THE OTHER PLAYER" (PLAYERS, with more machines) shows if a game goes quiet for a moment, and **PRESS
  START TO JOIN** until you have.
- **Coming back after a drop**: if the other player's game closes or their connection drops, the host's game stays
  open and waits for them with the same code; a joining player whose connection dropped is taken back to the host by
  itself. The two games then start over together from the title. (The host closing their game ends the match.) In a
  match of more machines, one leaving or dropping ends the match for all, saying who: the host waits again with the
  same code, the others go back to it by themselves, and it starts over when four are there again (whoever has the
  code can take the free place).
- **Win streaks**: in the fighting games, a player who wins two or more matches in a row against the other has it
  said under their life bar ("STREAK 3"), counted from when both have pressed Start, until the streak ends.
- **Names**: each player's name, as set on the launcher's Online page, is under their life bar (Hide names, on the
  Online page, hides them).
- **Chat**: press T on the keyboard, a gamepad's right stick click, or the touch screen's Chat button, type a line and
  press Enter (Esc, or the click again, puts it away). The Steam Deck and phones show their own on-screen keyboard.
  The last few lines show at the bottom left for a few seconds, and under them how to chat (as a match begins) and
  how to send or put away the line you're typing. Chat goes straight from one game to the other, as the
  controls do (through the host's to the others, with more machines): nothing of it passes through our service. While you type, the keyboard is the chat's, so a keyboard
  player's fighter stands still; a gamepad still plays.
- **Leaving a match**: Esc, or the gamepad's menu button, asks **Leave the match?** over the game (which goes on);
  Confirm leaves, Back stays.
- **Off during a match**: the cheats (on both sides, for a fair match), the in-game menu, save states, pause, the speed, run-ahead, the training mode and the
  other modes (endless battle, the viewers). A match ends when any player closes their game.
