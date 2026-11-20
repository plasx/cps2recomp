// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morphus56K. Part of cps2recomp.
//
// Marvel vs. Capcom: Cross Fever, the Dreamcast's mode for four people, on top of the toolkit's tag team (fight/tag.h;
// tag.cpp here says where the game keeps the character on point, and whose each character is). On the board a side's
// two characters both read the side's one panel, which in a Duo Team Attack (the partner in beside the character on
// point for a time) moves both. The Dreamcast's executable tests its Cross Fever flag in fifty places; those that change
// the fight are done here, each at the board's routine that the Dreamcast's does the same work in:
//
//   - Each character reads its own person's controller all the time: the Dreamcast reads a character's stream by its
//     byte +275 (the board's +281, 1 off point: 8C029E40) and turns the side's selector over where the bytes are swapped
//     (8C02CAA2 and on) and where a side starts with its second character (8C03AA0C), so that each stream stays with its
//     person. Here 01368E, by whose each character is (tag.cpp).
//   - The Duo Team Attack's time is counted down only while the game's count of steps is odd, so it lasts twice as long
//     (8C02CF20). Here 015F68.
//   - Every seventh step of a match each side's hyper combo gauge goes up by one, of the 0x90 a level (8C03606A: the
//     Dreamcast's gauge routine, the board's 01CD1E, for the first character of each side, when its own count of the
//     steps, 8C1E24A8, comes to seven). Here 01CEAA, with the same count.
//   - The character off point, when it is in the fight but not in a Duo Team Attack (coming in for a Variable Counter,
//     going off after an attack), has its own person's controls read and its moves looked for, unless the side's moves
//     are held (8C03025C passes it by in the other modes; the Dreamcast's step has nothing of the board's 00D4AC). Here
//     00D4AC and 018DB6.
//   - The taunt goes while the person on point holds Start (8C032674: that controller's Start as it is held, not as it
//     is pressed). Here 01A196. (In its versus modes the Dreamcast also wants the player's taunt button held alone,
//     8C4EBF9A, light punch unless changed, since Start alone pauses its game; here Start is the taunt, as on the board.)
//
// For Duo, both versions share the arcade rules for the attack's length (016E02: by the gauge's levels, and
// half the bar) and who stays in at its end (015FF0 goes by what each character is doing, as the Dreamcast's does).
// Every place is from the listing (tools/lst.py); traced with games/mvsc/movies/cross_fever.txt.
#include "alternative.h"
#include "state.h"
#include "tag.h"
#include "mvsc.h"
#include "../gauge_settings.h"

namespace cps2recomp {

int TagPick(uint32_t record); // tag.cpp
void ResetTagOrder();

namespace {
    using namespace mvsc;

    constexpr const char *kFeature = "cross_fever";
    // (issue #363: the gauges' filling by themselves, which competitive players may want off; the host's, as the mode)


    // On with tag team's people routed (co-op or 2 vs 2, and no mode of the sources' own)
    bool On() {
        return FeatureOn(kFeature) && tag::Routing();
    }

    // A person's controls as the game reads a panel (000D2E): the stick and the punches in the low byte, the kicks in
    // bits 8-10
    uint16_t PanelWord(uint16_t held) {
        return static_cast<uint16_t>((held & 0x7F) | (((held >> 7) & 7) << 8));
    }

    // Bytes of a character's record: 1 while it is off point (kept by the swaps); and the game's: bit n set while side n's
    // moves are held (FF4031: a tag on its way, 014ACC), bits 2 and 4 while nobody's are looked for (FF400F)
    constexpr uint32_t kOffPoint = 0x281;
    constexpr uint32_t kMovesHeld = 0xFF4031, kNoMoves = 0xFF400F;
    uint8_t g_tagHeld = 0x0F;
    // Each side's pending request is its bench owner's seat plus one (zero: none).
    uint8_t g_benchRequests[2] = {};
    uint8_t g_spentBenchChord = 0; // a consumed bench chord must not tag back on its release after entry
    enum : uint8_t { kOrdinary, kAfterResult, kFreshPicks };
    uint8_t g_reselect = kOrdinary;

    bool EveryoneReselects() {
        return On() && tag::Chosen() >= tag::kVersus && FeatureOn("cross_fever_reselect");
    }

    // Before 012798/0127A6 retain only the winner's bit, remember that both teams played.
    // The board must still finish the match, announce its winner and update the set.
    bool MatchDecided(M68K &) {
        g_reselect = EveryoneReselects() && Ram8(kPlayersIn) == 3 ? kAfterResult : kOrdinary;
        return false;
    }

    // At the end of results use the board's challenger-to-selection tail. It restores all four
    // records' human flags from the joined-team mask and enters 00AF46, the interactive selector.
    bool Reselect(M68K &c) {
        if (g_reselect != kAfterResult || Ram16(kGameStep) != kGameResult) return false;
        g_reselect = kOrdinary;
        if (!EveryoneReselects()) return false;
        g_reselect = kFreshPicks;
        SetRam8(kPlayersIn, 3);
        SetRam8(kJoining, 3);
        c.Push32(c.a[6]);
        c.Push32(c.a[4]); // MOVEM.L A4/A6,-(A7) at 0242AE; its tail restores both
        Spend(c, 24);
        c.pc = 0x024346;
        return true;
    }

    // If the losing team pressed Start during results, the stock challenger flow already
    // restored its human flags before calling 00AF46. Open the winning team's cursor too.
    bool ChallengerReselect(M68K &) {
        if (g_reselect != kAfterResult || Ram16(kGameStep) != kGameChoosing) return false;
        g_reselect = kOrdinary;
        if (EveryoneReselects() && Ram8(kPlayersIn) == 3) {
            SetRam8(kJoining, 3);
            g_reselect = kFreshPicks;
        }
        return false;
    }

    // 00AF86 rebuilds the mask of teams keeping their fighters each time an admission edge is
    // consumed. When both cursors must open, the second edge must not re-lock the previous winner.
    // Leave the winner's fighter record (+2B2), ranking and score intact.
    bool FreshPicks(M68K &c) {
        if (g_reselect != kFreshPicks || !EveryoneReselects() || Ram16(kGameStep) != kGameChoosing) return false;
        SetRam8(0xFF4160, 0);
        c.pc = 0x00AFB2; // process FF4102 through the stock code
        Spend(c, 16); // CLR.B at the start of this routine
        return true;
    }

    int BenchOwner(int side) {
        if (!tag::Routing() || (FeatureOn(kFeature) && !FeatureOn("cross_fever_partner_tag")) ||
            Ram16(kGameStep) != kGameMatch || Ram8(kFight) != kFighting ||
            !(Ram8(kPlayersIn) & (1 << side)) || ((Ram8(kDuoOn) | Ram8(kMovesHeld)) & (1 << side))) return -1;
        const uint32_t point = Player(side);
        const uint32_t partner = PartnerOf(side);
        if (Ram8(point + kOffPoint) || Ram8(partner) || !Ram16(point + kHealth) || !Ram16(partner + kHealth)) return -1;
        const int pick = TagPick(point);
        const int bench = tag::Person(side, 1 - pick);
        return bench != tag::Person(side, pick) ? bench : -1;
    }

    // 01945C is the BNE after the human point fighter's normal tag chord check at 019456.
    // The game has checked the partner's health and availability here. Let its remaining
    // eligibility check at 017476 and its own tag action run. A bench request never inserts
    // directions or attacks into the point fighter's command history.
    bool PartnerTag(M68K &c) {
        if (!g_benchRequests[0] && !g_benchRequests[1] && !g_spentBenchChord) return false;
        const uint32_t record = c.a[6] & 0xFFFFFF;
        const int side = SideOf(record);
        if (side < 0 || record != Player(side) || !tag::Routing()) return false;
        const int owner = tag::Person(side, TagPick(record));
        if (owner >= 0 && (g_spentBenchChord & (1 << owner))) {
            // The native matcher at 01A0C2 also counts falling edges over these three polls. Its
            // release of the bench owner's spent chord must not become a second tag on entering.
            const uint16_t history = Ram16(record + 0xD0) | Ram16(record + 0xD2) | Ram16(record + 0xE0);
            if (!(history & 0x440) && !(tag::Held(owner) & (kPunch3 | kKick3))) {
                g_spentBenchChord &= ~(1 << owner);
            } else if (!c.flagZ) {
                c.d[0] = 0;
                c.flagZ = true;
            }
        }
        if (!c.flagZ || !g_benchRequests[side] || g_benchRequests[side] != BenchOwner(side) + 1) return false;
        c.d[0] = 1;
        c.flagZ = false; // MOVEQ #1 from the normal successful chord matcher
        return false; // execute the stock BNE
    }

    // 019468: the native eligibility check at 017476 just returned. Carry rejects the tag
    // (among other things, during recovery). Consume this side's request only on a committed tag.
    bool TagAccepted(M68K &c) {
        const int side = SideOf(c.a[6] & 0xFFFFFF);
        if (!c.flagC && side >= 0 && g_benchRequests[side]) {
            g_spentBenchChord |= 1 << (g_benchRequests[side] - 1);
            g_benchRequests[side] = 0;
        }
        return false;
    }

    // Off point, outside its side's Duo Team Attack (kDuoOn)
    bool OffPoint(uint32_t record) {
        const int side = SideOf(record);
        return side >= 0 && Ram8(record + kOffPoint) != 0 && !((Ram8(kDuoOn) >> side) & 1);
    }

    // 01368E: a record a person plays (A6) reads its side's panel: the word into D0 (its high half 0), through the table
    // at 0136F4 (into A0: kPanel). In Cross Fever each record reads its own person's controls instead, whatever it is
    // doing (on point, beside the other in a Duo Team Attack or a team super, coming in, resting). So the person on point
    // is the one who tags out, as on the Dreamcast. A permitted bench tag is handled at the tag checker above.
    bool ReadPanel(M68K &c) {
        if (!On()) {
            return false;
        }
        const uint32_t record = c.a[6] & 0xFFFFFF;
        const int side = SideOf(record);
        if (side < 0) {
            return false;
        }
        const int person = tag::Person(side, TagPick(record));
        if (person < 0) {
            return false;
        }
        const uint16_t word = PanelWord(tag::Held(person));
        c.a[0] = kPanel[side];
        c.d[0] = word;
        // (the flags as move.w (a0),d0 leaves them, and X as the two add.w before it)
        c.flagN = (word & 0x8000) != 0;
        c.flagZ = word == 0;
        c.flagV = c.flagC = c.flagX = false;
        Spend(c, 50); // (the six instructions' time, moveq to move.w)
        c.pc = 0x01369E;
        return true;
    }

    // 015F68: a side's time for its Duo Team Attack (A4: kDuoTime's word) taken one off. In Cross Fever only on odd steps
    // (kStepCount); on even ones on to 015F6C, as when the time is held, the attack's bit set again.
    bool DuoTime(M68K &c) {
        if (!On() || FeatureOn("cross_fever_arcade_time") || (Ram8(kStepCount + 1) & 1)) {
            return false;
        }
        Spend(c, 20); // (subq.w and ble's time)
        c.pc = 0x015F6C;
        return true;
    }

    // The steps counted for the gauge, as the Dreamcast counts them (8C1E24A8): one more each time its 8C036052 runs
    // (once a step of a match, from the intro on: 8C0221E8, the board's 00CEE2), and in Cross Fever back to 0 at seven,
    // when both sides get their 1. (The Dreamcast's goes on counting outside Cross Fever; here it stops at seven, which
    // comes to the same.) Kept with save states and across matches, but cleared when the board boots.
    uint8_t g_gaugeSteps;

    // 000FAC: the board's boot calls its settings reader (007E7A; board.cpp hooks its return at 000FB2).
    // Machine::Reset clears board RAM. A rehost must also start the clock, tag edges and both controller
    // order selectors as a fresh process would, even when the previous host used different features.
    bool GaugeBoot(M68K &) {
        ResetTagOrder();
        g_reselect = kOrdinary;
        g_gaugeSteps = 0;
        g_tagHeld = 0x0F;
        g_benchRequests[0] = g_benchRequests[1] = g_spentBenchChord = 0;
        return false;
    }

    // 01CEAA, once a step of a match (00CEE2's jsr is the only way in: 00CEE8 is on the stack), before the health a
    // resting character gets back. In Cross Fever, at every seventh step (g_gaugeSteps), the gauge routine (01CD1E: D0
    // added to A6's side, with the board's own tests: in the fight, not in a Duo Team Attack, levels and their sound)
    // for player 1's first character and then player 2's, as the Dreamcast's 8C036052 does before its own part, which is
    // this routine. The game runs it as if called from here, and it comes back here: the registers are kept on the stack
    // under a long saying which call is done, so a save state taken in the middle goes on as it would have.
    constexpr uint32_t kFromTheStep = 0x00CEE8;
    constexpr uint32_t kFirstDone = 0x43460001, kSecondDone = 0x43460002; // ("CF")
    bool GaugeStep(M68K &c) {
        const uint32_t top = c.Read(c.a[7], 2);
        if (top == kFromTheStep) {
            if (g_gaugeSteps < 7) {
                g_gaugeSteps++;
            }
            if (!On() || !FeatureOn(mvsc::kAutoGauge) || g_gaugeSteps < 7) {
                return false;
            }
            g_gaugeSteps = 0;
            if (Ram32(kPlayer1 + kPartner) != kPlayer1 + kPartners ||
                Ram32(kPlayer1 + kPlayerSize + kPartner) != kPlayer1 + kPlayerSize + kPartners) {
                return false;
            }
            for (int n = 14; n >= 0; n--) { // (movem.l d0-d7/a0-a6,-(a7))
                c.Push32(n < 8 ? c.d[n] : c.a[n - 8]);
            }
            c.Push32(kFirstDone);
            c.Push32(0x01CEAA);
            c.a[6] = kPlayer1;
            c.d[0] = 1;
            Spend(c, 160);
            c.pc = 0x01CD1E;
            return true;
        }
        if (top == kFirstDone) {
            c.Write(c.a[7], 2, kSecondDone);
            c.Push32(0x01CEAA);
            c.a[6] = kPlayer1 + kPlayerSize;
            c.d[0] = 1;
            Spend(c, 40 + 132); // (and the registers' taking back after it: a hook that lets the block run takes no time)
            c.pc = 0x01CD1E;
            return true;
        }
        if (top == kSecondDone) {
            c.a[7] += 4;
            for (int n = 0; n < 15; n++) { // (movem.l (a7)+,d0-d7/a0-a6)
                const uint32_t value = c.Pop32();
                (n < 8 ? c.d[n] : c.a[n - 8]) = value;
            }
        }
        return false;
    }

    // 00D4AC: a record's controls at the start of its reading (the record in A6). For a character off point outside its
    // side's Duo Team Attack, the board clears its controls and goes past its reading, its moves and the rest of them (to
    // 00D5A4) while kFight's word is below 0x0A (00D4BC: cmpi.w; so while its byte is 0, not the 8 of a round being fought,
    // as when a Variable Counter's partner comes in), and otherwise reads them as any other's. In Cross Fever on to its
    // reading (00D4E6) in every case, as the Dreamcast does: the hook passes over only where the board would clear.
    bool PartnerControls(M68K &c) {
        if (!On() || !OffPoint(c.a[6] & 0xFFFFFF) || Ram16(kFight) >= 0x0A) {
            return false;
        }
        Spend(c, 74); // (the tests' time, as the board's code takes it)
        c.pc = 0x00D4E6;
        return true;
    }

    // 018DB6: a record's moves looked for (by its character, 018DD6), unless nobody's are (kNoMoves), or it is off point
    // outside its side's Duo Team Attack, or its side's moves are held (kMovesHeld). In Cross Fever the character off
    // point is passed by only when its side's moves are held.
    bool PartnerMoves(M68K &c) {
        const uint32_t record = c.a[6] & 0xFFFFFF;
        if (!On() || (Ram8(kNoMoves) & 0x14) || !OffPoint(record)) {
            return false;
        }
        const int side = SideOf(record);
        c.d[0] = (c.d[0] & 0xFFFFFF00) | static_cast<uint32_t>(side); // (move.b 0x2(a6),d0)
        c.flagZ = !((Ram8(kMovesHeld) >> side) & 1);
        Spend(c, 74);
        c.pc = c.flagZ ? 0x018DD6 : 0x018DEE;
        return true;
    }

    // 01A196: the board uses the team's Start edge (+26E/+26F). During Duo two different people
    // can play the team's two records, so read held Start and the configured attack from the actor's owner.
    bool Taunt(M68K &c) {
        if (!On()) {
            return false;
        }
        const uint32_t record = c.a[6] & 0xFFFFFF;
        const int side = SideOf(record);
        if (side < 0) {
            return false;
        }
        const int owner = tag::Person(side, TagPick(record));
        if (owner < 0) return false;
        const uint16_t held = tag::Held(owner);
        const uint8_t start = (held & kStart) ? 1 << side : 0;
        c.d[0] = (c.d[0] & 0xFFFFFF00) | start;
        c.d[1] = static_cast<uint32_t>(side);
        // USA Dreamcast 8C0326E6..8C03270C compares the entire fighter input word with one attack.
        // Directions and extra attacks disqualify it. Start, coin and unused bits are outside that word.
        constexpr uint16_t chordButton[] = {0, 0x10, 0x20, 0x40, 0x100, 0x200, 0x400};
        const int choice = FeatureValue("cross_fever_taunt");
        const bool chord = choice > 0 && choice <= 6;
        c.flagZ = !start || (chord && PanelWord(held) != chordButton[choice]);
        Spend(c, 50);
        c.pc = c.flagZ ? 0x01A21C : 0x01A1B0;
        return true;
    }
} // namespace

void CrossFeverInputs(const uint16_t *players) {
    if (g_reselect == kFreshPicks && Ram16(mvsc::kGameStep) == mvsc::kGameMatch) g_reselect = kOrdinary;
    constexpr auto chord = kPunch3 | kKick3;
    uint8_t held = 0;
    for (int i = 0; i < Machine::kMaxPlayers; i++) {
        if ((players[i] & chord) == chord) held |= 1 << i;
    }
    const uint8_t pressed = held & ~g_tagHeld;
    g_spentBenchChord &= ~pressed; // a new press after release is the new owner's own command
    if (g_spentBenchChord && (Ram16(mvsc::kGameStep) != mvsc::kGameMatch || Ram8(mvsc::kFight) != mvsc::kFighting))
        g_spentBenchChord = 0;
    for (int side = 0; side < 2; ++side) {
        if (!g_benchRequests[side] && !pressed) continue;
        const int owner = BenchOwner(side);
        if (owner < 0 || !(held & (1 << owner))) g_benchRequests[side] = 0;
        else if (pressed & (1 << owner)) g_benchRequests[side] = owner + 1;
        else if (g_benchRequests[side] != owner + 1) g_benchRequests[side] = 0;
    }
    g_tagHeld = held;
}

void CrossFeverState(Archive &ar) {
    const bool hasTagEdges = ar.version == 0 || ar.version >= 14 || (ar.version == 13 && !ar.v13BoardDma);
    // Ignore held inputs until release when loading a state predating bench-tag edges.
    if (!ar.Saving() && !hasTagEdges) {
        g_tagHeld = 0x0F;
        g_benchRequests[0] = g_benchRequests[1] = g_spentBenchChord = 0;
        g_reselect = kOrdinary;
    }
    // Version 11 existed both with this preview's clock and without it in upstream MVC.
    if (ar.version != 0 && (ar.version < 11 || (ar.version == 11 && !ar.Saving() && !ar.NextMark(0x43465653)))) {
        g_gaugeSteps = 0;
        return;
    }
    ar.Mark(0x43465653); // CFVS
    ar.Pod(g_gaugeSteps);
    if (hasTagEdges) {
        // Early v13 wrote a four-bit held mask followed by transient edges with no bench
        // ownership. Preserve their widths; the mark distinguishes two valid owner slots on load.
        uint8_t held = g_tagHeld | (g_spentBenchChord << 4);
        uint8_t requests = 0x80 | g_benchRequests[0] | (g_benchRequests[1] << 3);
        ar.Pod(held);
        ar.Pod(requests);
        if (!ar.Saving()) {
            g_tagHeld = held & 0x0F;
            g_spentBenchChord = (requests & 0x80) ? held >> 4 : 0;
            for (int side = 0; side < 2; ++side) {
                const uint8_t seat = (requests >> (3 * side)) & 7;
                g_benchRequests[side] = (requests & 0x80) && seat <= Machine::kMaxPlayers ? seat : 0;
            }
        }
        ar.Pod(g_reselect);
    }
}

void RegisterCrossFever() {
    TheGame().migrateSettings = mvsc::MigrateGaugeSettings;
    TheGame().migrateOption = mvsc::MigrateGaugeOption;
    TheGame().pauseOnPadRemoval = [](int seat) {
        if (seat < 0 || !On() || !FeatureOn("cross_fever_disconnect_pause") ||
            static_cast<int8_t>(Ram8(mvsc::kPlaying)) <= 0 || Ram16(mvsc::kGameStep) != mvsc::kGameMatch ||
            Ram8(mvsc::kFight) != mvsc::kFighting) return false;
        for (int side = 0; side < 2; ++side) {
            if ((Ram8(mvsc::kPlayersIn) & (1 << side)) &&
                (tag::Person(side, 0) == seat || tag::Person(side, 1) == seat)) return true;
        }
        return false;
    };
    AddHook(HookAt(0x000FAC), "CrossFeverGaugeBoot", GaugeBoot);
    AddHook(HookAt(0x01368E), "CrossFeverPanel", ReadPanel);
    AddHook(HookAt(0x012786), "CrossFeverMatchDecided", MatchDecided);
    AddHook(HookAt(0x0D39EE), "CrossFeverReselect", Reselect);
    AddHook(HookAt(0x00AF86), "CrossFeverFreshPicks", FreshPicks);
    AddHook(HookAt(0x00AF46), "CrossFeverChallengerReselect", ChallengerReselect);
    AddHook(HookAt(0x01945C), "CrossFeverPartnerTag", PartnerTag);
    AddHook(HookAt(0x019468), "CrossFeverTagAccepted", TagAccepted);
    AddHook(HookAt(0x015F68), "CrossFeverDuoTime", DuoTime);
    AddHook(HookAt(0x01CEAA), "CrossFeverGauge", GaugeStep);
    AddHook(HookAt(0x00D4AC), "CrossFeverPartnerControls", PartnerControls);
    AddHook(HookAt(0x018DB6), "CrossFeverPartnerMoves", PartnerMoves);
    AddHook(HookAt(0x01A196), "CrossFeverTaunt", Taunt);
    TheGame().features.push_back({kFeature, modes::kModes, "Cross Fever",
                                  "Choose a Tag team layout first. During Duo, each human controls their chosen fighter. "
                                  "The host chooses the meter, Duo duration and tag rules for everyone.",
                                  kFeatureMatch});
    TheGame().features.push_back({mvsc::kAutoGauge, modes::kModes, "Cross Fever gauges",
                                  "In Cross Fever, whether both teams' hyper combo gauges also fill slowly by themselves, as "
                                  "on the Dreamcast, or only by the fighting, as in the arcade's other modes.",
                                  kFeatureMatch, {"Only by fighting", "Fill by themselves"}});
    TheGame().features.push_back({"cross_fever_arcade_time", modes::kModes, "Cross Fever Duo time",
                                  "Dreamcast counts down Duo time every other game step. Arcade uses the original countdown.",
                                  kFeatureMatch, {"Dreamcast", "Arcade"}});
    TheGame().features.push_back({"cross_fever_taunt", modes::kModes, "Cross Fever taunt",
                                  "Start repeats the taunt while held. For Dreamcast controls choose Start + LP, "
                                  "or choose another attack button. Hold only that attack with Start and leave the stick neutral.",
                                  kFeatureMatch, {"Start", "Start + LP", "Start + MP", "Start + HP",
                                                  "Start + LK", "Start + MK", "Start + HK"}});
    TheGame().features.push_back({"cross_fever_reselect", modes::kModes, "Cross Fever rematch",
                                  "After a versus match, Arcade continue keeps the winning team. Everyone reselects "
                                  "returns both teams to character select. The set keeps its score.",
                                  kFeatureMatch, {"Arcade continue", "Everyone reselects"}});
    TheGame().features.push_back({"cross_fever_disconnect_pause", modes::kModes, "Pause on controller disconnect",
                                  "In an offline Cross Fever fight, open the pause menu if a player's controller disconnects. "
                                  "Reconnect the controller and close the menu to resume.", 0});
    TheGame().features.push_back({"cross_fever_partner_tag", modes::kModes, "Cross Fever tag",
                                  "Press HP+HK to tag. Point follows the Dreamcast. Either also lets the resting "
                                  "teammate request a normal tag with a fresh HP+HK press outside Duo.",
                                  kFeatureMatch, {"Point", "Either"}});
}

} // namespace cps2recomp
