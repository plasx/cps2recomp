// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Morphus56K and contributors. Part of cps2recomp.
// ROM-free regression for MVC's Cross Fever gauge clock across board resets and save states.
// Link the production MVC hooks and tag routing. Only the surrounding bus, feature, HUD and hook services are stand-ins;
// the registered hooks and their state serialization are the game's actual implementation.
#include "game.h"
#include "lift.h"
#include "state.h"
#include "tag.h"
#include "mvsc.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string>

namespace {
    using namespace cps2recomp;
    std::array<uint8_t, 0x10000> memory;
    std::map<uint32_t, HookFn> hooks;
    bool enabled = true, routing = true, arcadeTime = false;
    bool benchTag = false, disconnectPause = false;
    int tauntButton = 0;
    int teamOrder = 0;
    bool reselect = false;
    tag::Mode teamMode = tag::kVersus;
    int gaugeChoice = 1; // These clock tests explicitly select Fill by themselves.

    void Require(bool value, const std::string &why) {
        if (!value) {
            throw std::runtime_error(why);
        }
    }

    struct Bus : M68KBus {
        uint8_t Read8(uint32_t address) override { return memory[address & 0xFFFF]; }
        uint16_t Read16(uint32_t address) override { return static_cast<uint16_t>(Read8(address) << 8 | Read8(address + 1)); }
        void Write8(uint32_t address, uint8_t value) override { memory[address & 0xFFFF] = value; }
        void Write16(uint32_t address, uint16_t value) override {
            Write8(address, static_cast<uint8_t>(value >> 8));
            Write8(address + 1, static_cast<uint8_t>(value));
        }
        uint16_t Fetch16(uint32_t address) override { return Read16(address); }
    } bus;
} // namespace

namespace cps2recomp {
    void RegisterCrossFever();
    void CrossFeverState(Archive &ar);
    void CrossFeverInputs(const uint16_t *players);
    void RegisterTag();
    void TagState(Archive &ar);
    int TagPick(uint32_t record);

    Game &TheGame() { static Game game; return game; }
    bool FeatureOn(const char *name) {
        const std::string key(name);
        if (key == "cross_fever_auto_gauge") return gaugeChoice != 0;
        if (key == "cross_fever_arcade_time") return arcadeTime;
        if (key == "cross_fever_partner_tag") return benchTag;
        if (key == "cross_fever_reselect") return reselect;
        if (key == "cross_fever_disconnect_pause") return disconnectPause;
        return key == "cross_fever" && enabled;
    }
    int FeatureValue(const char *name) {
        const std::string key(name);
        if (key == tag::kFeature) return teamMode;
        if (key == tag::kTeamOrder) return teamOrder;
        return key == "cross_fever_taunt" ? tauntButton : 0;
    }
    uint8_t *Ram(uint32_t address) { return &memory[address & 0xFFFF]; }
    bool BattleMode() { return false; }
    namespace session { bool InSession() { return !routing; } }
    namespace modes {
        bool AttractMode() { return false; }
        bool StageViewer() { return false; }
    }
    namespace marvel { bool ModelViewer() { return false; } }
    void AddHook(uint32_t address, const char *, HookFn fn) {
        Require(hooks.emplace(address, fn).second, "duplicate hook address");
    }
    static std::vector<HudLine> testHud;
    void SetHud(std::vector<HudLine> lines) { testHud = std::move(lines); }
    const std::vector<HudLine> &Hud() { return testHud; }
    uint32_t M68K::Read(uint32_t address, unsigned size) {
        uint32_t value = 0;
        for (unsigned n = 0; n < (1u << size); n++) {
            value = value << 8 | bus.Read8(address + n);
        }
        return value;
    }
    void M68K::Write(uint32_t address, unsigned size, uint32_t value) {
        for (unsigned n = 1u << size; n > 0; n--) {
            bus.Write8(address + n - 1, static_cast<uint8_t>(value));
            value >>= 8;
        }
    }
} // namespace cps2recomp

namespace {
    // The native gauge routine is not executed here. Walk only its two returns, checking that the hook requested
    // exactly one point for each side and restored its stack. The boolean is the observable credit opportunity.
    bool Step() {
        M68K cpu(bus);
        cpu.a[7] = 0xFF8000;
        cpu.Write(cpu.a[7], 2, 0x00CEE8);
        cpu.pc = 0x01CEAA;
        const auto hook = hooks.at(cpu.pc);
        if (!hook(cpu)) {
            return false;
        }
        Require(cpu.pc == 0x01CD1E && cpu.d[0] == 1 && cpu.a[6] == mvsc::kPlayer1, "first gauge request");
        cpu.pc = cpu.Pop32();
        Require(cpu.pc == 0x01CEAA && hook(cpu), "first gauge return");
        Require(cpu.pc == 0x01CD1E && cpu.d[0] == 1 && cpu.a[6] == mvsc::kPlayer1 + mvsc::kPlayerSize,
                "second gauge request");
        cpu.pc = cpu.Pop32();
        Require(cpu.pc == 0x01CEAA && !hook(cpu) && cpu.a[7] == 0xFF8000, "second gauge return");
        return true;
    }

    void ClearOldState() {
        std::vector<uint8_t> bytes;
        Archive ar(bytes, false);
        ar.version = 10;
        CrossFeverState(ar);
        Require(ar.Ok(), "legacy state must start a fresh gauge clock");
        memory.fill(0);
        enabled = routing = true;
    }

    void MakeRecords() {
        SetRam32(mvsc::kPlayer1 + mvsc::kPartner, mvsc::kPlayer1 + mvsc::kPartners);
        SetRam32(mvsc::kPlayer1 + mvsc::kPlayerSize + mvsc::kPartner,
                 mvsc::kPlayer1 + mvsc::kPlayerSize + mvsc::kPartners);
    }

    void Boot() {
        M68K cpu(bus);
        cpu.pc = 0x000FAC; // verified stock boot call: jsr $007E7A (settings read)
        cpu.cycles = 1234;
        for (unsigned n = 0; n < 8; n++) {
            cpu.d[n] = n + 11;
            cpu.a[n] = n + 21;
        }
        cpu.flagC = cpu.flagX = cpu.flagN = true;
        const auto before = cpu;
        const auto ramBefore = memory;
        const auto found = hooks.find(cpu.pc);
        // Without a hook the board simply executes its stock call, which cannot clear the host's static clock.
        if (found != hooks.end()) {
            Require(!found->second(cpu), "boot must let the stock settings call run");
        }
        Require(cpu.pc == before.pc && cpu.cycles == before.cycles &&
                    std::equal(std::begin(cpu.d), std::end(cpu.d), std::begin(before.d)) &&
                    std::equal(std::begin(cpu.a), std::end(cpu.a), std::begin(before.a)) &&
                    cpu.flagC == before.flagC && cpu.flagV == before.flagV && cpu.flagZ == before.flagZ &&
                    cpu.flagN == before.flagN && cpu.flagX == before.flagX && memory == ramBefore,
                "boot clock reset must not change the CPU or board memory");
    }

    std::vector<bool> Credits(unsigned steps = 21) {
        std::vector<bool> result;
        while (steps--) {
            result.push_back(Step());
        }
        return result;
    }

    std::vector<bool> AfterBoot(unsigned prior, unsigned intro, bool feature = true, bool route = true) {
        ClearOldState();
        for (unsigned n = 0; n < prior; n++) { Step(); }
        enabled = feature;
        routing = route;
        Boot();
        enabled = routing = true;
        // Intro steps still advance the clock even before the partner records are valid; they must not be
        // mistaken for a reset that erases a previous run's phase.
        for (unsigned n = 0; n < intro; n++) { Step(); }
        MakeRecords();
        return Credits();
    }

    void StateVersions() {
        // The preview and upstream used 11 for different additions. Only the preview contains MVC's TAOR/CFVS.
        for (unsigned version : {11u, 12u, 13u, 14u}) {
            ClearOldState();
            MakeRecords();
            Credits(3);
            std::vector<uint8_t> bytes;
            Archive save(bytes, true);
            save.version = version;
            save.Mark(0x54414F52);
            uint8_t swaps[2] = {1, 0};
            save.Pod(swaps);
            CrossFeverState(save);
            save.Mark(0x44535447); // the following guest-stage part must remain readable
            Boot();
            Archive load(bytes, false);
            load.version = version;
            TagState(load);
            CrossFeverState(load);
            load.Mark(0x44535447);
            Require(load.Ok() && load.AtEnd() && TagPick(mvsc::kPlayer1) == 1 && TagPick(mvsc::kPlayer1 + mvsc::kPlayerSize) == 0,
                    "marked MVC state lost controller ownership");
            Require(!Step() && !Step() && !Step() && Step(), "marked MVC state lost its gauge phase");
        }
        // An upstream MVC 11 state goes straight to the next section, without either preview section.
        std::vector<uint8_t> upstream;
        Archive save(upstream, true);
        save.Mark(0x44535447);
        Credits(3);
        Archive load(upstream, false);
        load.version = 11;
        TagState(load);
        CrossFeverState(load);
        Require(load.NextMark(0x44535447), "absent preview parts consumed the upstream section");
        load.Mark(0x44535447);
        Require(load.Ok() && TagPick(mvsc::kPlayer1) == 0, "upstream state did not reset preview ownership");
        for (unsigned n = 0; n < 6; n++) { Require(!Step(), "upstream state retained a stale clock"); }
        Require(Step(), "upstream state's fresh clock did not reach seven");
        // New states require their parts: a truncated or missing clock is an error, not a legacy default.
        for (auto bytes : {std::vector<uint8_t>{}, std::vector<uint8_t>{1, 2, 3}, upstream}) {
            Archive broken(bytes, false);
            broken.version = 12;
            Require(!broken.NextMark(0x43465653), "invalid clock mark accepted");
            CrossFeverState(broken);
            Require(!broken.Ok(), "new state silently accepted a missing clock");
        }
    }

    void ReselectRules() {
        M68K cpu(bus);
        auto result = [&](uint8_t humans) {
            SetRam8(mvsc::kPlayersIn, humans);
            Require(!hooks.at(0x012786)(cpu), "must let the game record the winner");
            SetRam16(mvsc::kGameStep, mvsc::kGameResult);
            SetRam8(mvsc::kPlayersIn, 1); // stock winner after the result
            SetRam8(mvsc::kJoining, 0);
        };
        auto selecting = [&]() {
            cpu.a[7] = 0xFF8000;
            const bool entered = hooks.at(0x0D39EE)(cpu);
            return entered && cpu.pc == 0x024346 && Ram8(mvsc::kPlayersIn) == 3 && Ram8(mvsc::kJoining) == 3;
        };
        ClearOldState();
        for (auto mode : {tag::kOff, tag::kCoop, tag::kVersus, tag::kTwoVsOne, tag::kOneVsTwo}) {
            teamMode = mode;
            for (bool choice : {false, true}) {
                reselect = choice;
                for (uint8_t humans : {1, 2, 3}) {
                    result(humans);
                    Require(selecting() == (mode >= tag::kVersus && choice && humans == 3),
                            "only a completed human versus match can reselect");
                }
            }
        }
        result(3);
        std::vector<uint8_t> bytes;
        Archive save(bytes, true);
        save.version = 13;
        CrossFeverState(save);
        Boot();
        Require(!selecting(), "warm boot retained a pending reselection");
        Archive load(bytes, false);
        load.version = 13;
        CrossFeverState(load);
        Require(load.Ok() && selecting(), "save/load lost pending reselection");
        SetRam8(mvsc::kPlayersIn, 1);
        Require(!selecting(), "same result triggered a second reselection");
        SetRam16(mvsc::kGameStep, mvsc::kGameChoosing);
        SetRam8(0xFF4160, 3);
        Require(hooks.at(0x00AF86)(cpu) && cpu.pc == 0x00AFB2 && Ram8(0xFF4160) == 0,
                "stock keep-winner mask locked a rematch cursor");
        SetRam16(mvsc::kGameStep, mvsc::kGameMatch);
        const uint16_t held[4] = {};
        CrossFeverInputs(held);
        SetRam16(mvsc::kGameStep, mvsc::kGameChoosing);
        Require(!hooks.at(0x00AF86)(cpu), "fresh-pick override leaked into another selection");
        result(3);
        SetRam8(mvsc::kPlayersIn, 3);
        SetRam16(mvsc::kGameStep, mvsc::kGameChoosing);
        Require(!hooks.at(0x00AF46)(cpu) && Ram8(mvsc::kJoining) == 3,
                "early challenger Start did not open both teams in the stock selector");
        result(3);
        reselect = false;
        Require(!selecting(), "disabled option changed arcade continuation");
        reselect = true;
        Require(!selecting(), "re-enabling the option reused a previous result");
        teamMode = tag::kVersus;
        reselect = false;
    }

    void TeamOrder() {
        struct Layout {
            tag::Mode mode;
            std::array<int, 4> owners;  // left picks, then right picks
        };
        const Layout layouts[] = {{tag::kOff, {0, -1, 1, -1}}, {tag::kCoop, {0, 1, -1, -1}},
                                  {tag::kVersus, {0, 2, 1, 3}}, {tag::kTwoVsOne, {0, 2, 1, 1}},
                                  {tag::kOneVsTwo, {0, 0, 1, 2}}};
        for (int order : {0, 1, 2}) {
            teamOrder = order;
            for (const auto &layout : layouts) {
                teamMode = layout.mode;
                auto wanted = layout.owners;
                if (teamMode == tag::kVersus && order == 1) wanted = {0, 1, 2, 3};
                for (int slot = 0; slot < 4; ++slot)
                    Require(TheGame().matchOwner(slot / 2, slot % 2) == wanted[slot],
                            "roster and game must agree on the physical player for each fighter");
            }
        }
        teamOrder = 0;
        teamMode = tag::kVersus;
    }

    void PartnerTagRequests(int order) {
        teamOrder = order;
        ClearOldState();
        Boot();
        SetRam16(mvsc::kGameStep, mvsc::kGameMatch);
        SetRam8(mvsc::kFight, mvsc::kFighting);
        SetRam8(mvsc::kPlayersIn, 3);
        for (unsigned slot = 0; slot < 4; ++slot) {
            const uint32_t actor = mvsc::kPlayer1 + slot * mvsc::kPlayerSize;
            SetRam8(actor + mvsc::kWhichPick, slot / 2);
            SetRam8(actor + 0x281, slot / 2);
            SetRam16(actor + mvsc::kHealth, mvsc::kHealthFull);
        }
        uint16_t held[4] = {};
        constexpr uint16_t chord = kPunch3 | kKick3;
        auto sample = [&] {
            uint16_t raw[Machine::kMaxPlayers];
            std::copy(std::begin(held), std::end(held), raw);
            CrossFeverInputs(raw);
            tag::RouteInputs(raw);
        };
        auto pulse = [&](int person, uint16_t buttons) {
            held[person] = 0;
            sample();
            held[person] = buttons;
            sample();
        };
        auto requested = [&](int side, bool accepts = true) {
            M68K cpu(bus);
            cpu.a[6] = mvsc::Player(side);
            cpu.flagZ = true; // native point matcher did not recognize a chord
            Require(!hooks.at(0x01945C)(cpu), "native tag eligibility and BNE must run");
            if (cpu.flagZ || cpu.d[0] != 1) return false;
            cpu.flagC = !accepts; // 017476's result
            Require(!hooks.at(0x019468)(cpu) && cpu.flagC == !accepts, "preserve the native tag decision");
            return true;
        };
        const int benches[2] = {order == 1 ? 1 : 2, 3};
        for (int side : {0, 1}) {
            const int person = benches[side];
            benchTag = false;
            enabled = false;
            pulse(person, chord);
            Require(requested(side) && !requested(side), "classic Tag team lost its fresh bench request");
            routing = false;
            pulse(person, chord);
            Require(!requested(side), "stock arcade accepted a bench request");
            routing = enabled = true;
            pulse(person, chord);
            Require(!requested(side), "default Point accepted bench tag");
            benchTag = true;
            pulse(person, kPunch3);
            Require(!requested(side), "one button triggered tag");
            pulse(person, chord);
            sample(); // a blocked or recovering point did not reach the matcher on the press frame
            Require(requested(side, false), "held bench request was lost before reaching native eligibility");
            sample();
            Require(!requested(1 - side), "opposing team received bench tag");
            Require(requested(side) && !requested(side), "fresh chord must be consumed once");
            sample();
            Require(!requested(side), "consumed request repeated while still held");
            const uint32_t previousPoint = mvsc::Player(side), incoming = mvsc::PartnerOf(side);
            SetRam32(mvsc::kOnPoint + side * 8, incoming);
            SetRam8(incoming + 0x281, 0);
            SetRam8(previousPoint + 0x281, 1);
            auto pointMatched = [&] {
                M68K cpu(bus);
                cpu.a[6] = incoming;
                cpu.flagZ = false;
                cpu.d[0] = 1; // 01A0C2 recognized a transition (release also matches)
                Require(!hooks.at(0x01945C)(cpu), "do not replace the native tag branch");
                return !cpu.flagZ && cpu.d[0] == 1;
            };
            SetRam16(incoming + 0xD0, 0x440);
            SetRam16(incoming + 0xD2, 0x440);
            Require(!pointMatched(), "spent bench chord repeated on point entry");
            held[person] = 0;
            sample();
            SetRam16(incoming + 0xD0, 0);
            Require(!pointMatched(), "releasing the spent bench chord tagged back");
            SetRam16(incoming + 0xD2, 0);
            SetRam16(incoming + 0xE0, 0x440);
            Require(!pointMatched(), "last release edge tagged back");
            std::vector<uint8_t> afterEntry;
            Archive saveSpent(afterEntry, true);
            CrossFeverState(saveSpent);
            held[person] = chord;
            sample();
            Require(pointMatched(), "fresh chord from the new point owner must work");
            Archive loadSpent(afterEntry, false);
            loadSpent.version = 13;
            CrossFeverState(loadSpent);
            held[person] = 0;
            sample();
            Require(loadSpent.Ok() && !pointMatched(), "save/load replayed the spent release");
            SetRam16(incoming + 0xE0, 0);
            Require(pointMatched(), "native matcher not restored after its history cleared");
            SetRam32(mvsc::kOnPoint + side * 8, previousPoint);
            SetRam8(incoming + 0x281, 1);
            SetRam8(previousPoint + 0x281, 0);
            pulse(person, chord);
            Require(requested(side, false), "pending request absent before release");
            held[person] = kPunch3;
            sample();
            Require(!requested(side), "released chord waited past native eligibility");
            pulse(person, chord);
            SetRam8(mvsc::kDuoOn, 1 << side);
            sample();
            pulse(person, chord);
            Require(!requested(side), "own Duo accepted bench tag");
            sample();
            SetRam8(mvsc::kDuoOn, 0);
            Require(!requested(side), "held chord crossed Duo expiry");
            SetRam8(mvsc::kDuoOn, 1 << (1 - side));
            pulse(person, chord);
            Require(requested(side), "opponent Duo suppressed team tag");
            SetRam8(mvsc::kDuoOn, 0);
            pulse(person, chord);
            teamMode = side == 0 ? tag::kOneVsTwo : tag::kTwoVsOne;
            sample();
            Require(!requested(side), "solo team used partner override");
            teamMode = tag::kVersus;
            Require(!requested(side), "benched owner regained a request after changing roles");
            pulse(person, chord);
            const uint32_t partner = mvsc::PartnerOf(side);
            SetRam8(partner, 1); // an off-point fighter still active in a counter/departure
            sample();
            SetRam8(partner, 0);
            sample();
            Require(!requested(side), "an active teammate's command became a bench tag on departure");
            for (const auto actor : {partner, mvsc::Player(side)}) {
                pulse(person, chord);
                SetRam16(actor + mvsc::kHealth, 0);
                sample();
                SetRam16(actor + mvsc::kHealth, mvsc::kHealthFull);
                sample();
                Require(!requested(side), "held request crossed a KO");
            }
            pulse(person, chord);
            SetRam32(mvsc::kOnPoint + 8 * side, partner);
            SetRam8(partner + 0x281, 0);
            sample();
            SetRam32(mvsc::kOnPoint + 8 * side, partner - mvsc::kPartners);
            SetRam8(partner + 0x281, 1);
            sample();
            Require(!requested(side), "held request survived point replacement");
            pulse(person, chord);
            SetRam16(mvsc::kGameStep, mvsc::kGameResult);
            sample();
            SetRam16(mvsc::kGameStep, mvsc::kGameMatch);
            Require(!requested(side), "held request crossed results into a new round");
            pulse(person, chord);
            std::vector<uint8_t> bytes;
            Archive save(bytes, true);
            CrossFeverState(save);
            Require(requested(side), "could not consume prior to restore");
            Archive load(bytes, false);
            load.version = 13;
            CrossFeverState(load);
            Require(load.Ok() && requested(side), "unconsumed request lost on rollback");
            sample();
            Require(!requested(side), "held chord replayed after restore");
            // A v13 preview wrote an unqualified edge in the old second byte. Its owner and
            // side cannot be recovered. A held button must be released before a new request.
            std::vector<uint8_t> old;
            Archive oldSave(old, true);
            oldSave.Mark(0x43465653);
            uint8_t oldClock = 0, oldHeld = 1 << person, oldEdge = oldHeld, oldResult = 0;
            oldSave.Pod(oldClock);
            oldSave.Pod(oldHeld);
            oldSave.Pod(oldEdge);
            oldSave.Pod(oldResult);
            Archive oldLoad(old, false);
            oldLoad.version = 13;
            CrossFeverState(oldLoad);
            sample();
            Require(oldLoad.Ok() && !requested(side), "legacy one-frame edge became a held request");
            // Upstream v13 kept the gauge only. A request pending in the running game must be cleared,
            // even if the physical chord stays down across loading.
            std::vector<uint8_t> boardTiming;
            Archive writeBoardTiming(boardTiming, true);
            writeBoardTiming.Mark(0x43465653);
            uint8_t gauge = 3;
            writeBoardTiming.Pod(gauge);
            writeBoardTiming.Mark(0x44535447);
            pulse(person, chord);
            Archive loadBoardTiming(boardTiming, false);
            loadBoardTiming.version = 13;
            loadBoardTiming.v13BoardDma = true;
            CrossFeverState(loadBoardTiming);
            loadBoardTiming.Mark(0x44535447);
            sample();
            Require(loadBoardTiming.Ok() && loadBoardTiming.AtEnd() && !requested(side),
                    "upstream v13 consumed the next section or kept a held bench request");
            pulse(person, chord);
            Require(requested(side), "upstream v13 suppressed a new press after release");
            Archive truncated(boardTiming, false);
            truncated.version = 14;
            CrossFeverState(truncated);
            truncated.Mark(0x44535447);
            Require(!truncated.Ok(), "v14 accepted the shorter upstream v13 Cross Fever part");
            pulse(person, chord);
            Boot();
            sample();
            Require(!requested(side), "held chord survived board reset");
        }
        benchTag = false;
        teamOrder = 0;
    }

    void TauntOwners() {
        const uint16_t attacks[] = {0, kPunch1, kPunch2, kPunch3, kKick1, kKick2, kKick3};
        for (int order : {0, 1}) {
            teamOrder = order;
            for (auto mode : {tag::kVersus, tag::kTwoVsOne, tag::kOneVsTwo, tag::kCoop}) {
                teamMode = mode;
                for (int slot = 0; slot < 4; ++slot) {
                    const int side = slot % 2, pick = slot / 2;
                    const uint32_t actor = mvsc::kPlayer1 + slot * mvsc::kPlayerSize;
                    SetRam8(actor + mvsc::kWhichPick, pick);
                    SetRam8(mvsc::kDuoOn, pick ? 3 : 0);
                    const int owner = tag::Person(side, pick);
                    const int partner = tag::Person(side, 1 - pick);
                    M68K cpu(bus);
                    cpu.a[6] = actor;
                    if (owner < 0) {
                        Require(!hooks.at(0x01A196)(cpu), "human taunt intercepted a CPU fighter");
                        continue;
                    }
                    auto check = [&](uint16_t own, uint16_t other, bool expect) {
                        uint16_t players[Machine::kMaxPlayers] = {};
                        // Give the team panel the opposite Start and the fighter a stale attack.
                        // Neither can substitute for the two inputs from the actor's own controller.
                        SetRam8(actor + 0x26E, (own & kStart) ? 0 : (1 << side));
                        SetRam16(actor + 0xD0, kPunch3);
                        if (partner != owner) players[partner] = other;
                        players[owner] = own;
                        tag::RouteInputs(players);
                        Require(hooks.at(0x01A196)(cpu) && (cpu.pc == 0x01A1B0) == expect,
                                "Cross Fever taunt did not use the actor owner's Start and exact attack mask");
                    };
                    for (tauntButton = 0; tauntButton <= 6; ++tauntButton) {
                        const auto attack = attacks[tauntButton];
                        check(kStart | attack, 0, true);
                        check(kStart | kCoin | attack | 0x8000, 0, true); // unmapped bits carry no fighter command
                        check(attack, kStart, false);
                        check(kStart, attack, !tauntButton);
                        check(kStart | attack | kLeft, 0, !tauntButton);
                        check(kStart | attack | kPunch1 | kPunch2, 0, !tauntButton);
                    }
                }
            }
        }
        tauntButton = teamOrder = 0;
        teamMode = tag::kVersus;
        SetRam8(mvsc::kDuoOn, 0);
    }

    void DisconnectPause() {
        auto pause = TheGame().pauseOnPadRemoval;
        Require(pause, "missing local disconnect callback");
        SetRam16(mvsc::kGameStep, mvsc::kGameMatch);
        SetRam8(mvsc::kFight, mvsc::kFighting);
        SetRam8(mvsc::kPlaying, 1);
        SetRam8(mvsc::kPlayersIn, 3);
        enabled = routing = true;
        disconnectPause = false;
        Require(!pause(0), "disconnect pause should be opt in");
        disconnectPause = true;
        for (const auto mode : {tag::kCoop, tag::kVersus, tag::kTwoVsOne, tag::kOneVsTwo}) {
            teamMode = mode;
            const int size = mode == tag::kVersus ? 4 : (mode == tag::kCoop ? 2 : 3);
            for (int seat = -1; seat < 6; ++seat)
                Require(pause(seat) == (seat >= 0 && seat < size), "wrong seat paused offline fight");
        }
        teamMode = tag::kVersus;
        SetRam8(mvsc::kPlayersIn, 1);
        for (int order : {0, 1}) {
            teamOrder = order;
            for (int seat = 0; seat < 4; ++seat)
                Require(pause(seat) == (seat == 0 || seat == (order ? 1 : 2)), "unjoined team could pause the fight");
        }
        teamOrder = 0;
        SetRam8(mvsc::kPlayersIn, 3);
        SetRam8(mvsc::kPlaying, 0);
        Require(!pause(0), "stock attract fight enabled disconnect rule");
        SetRam8(mvsc::kPlaying, 1);
        enabled = false;
        Require(!pause(0), "ordinary tag play enabled Cross Fever disconnect rule");
        enabled = true;
        routing = false;
        Require(!pause(0), "inactive routing enabled disconnect rule");
        routing = true;
        SetRam8(mvsc::kFight, 0);
        Require(!pause(0), "fight intro enabled disconnect rule");
        SetRam8(mvsc::kFight, mvsc::kFighting);
        SetRam16(mvsc::kGameStep, mvsc::kGameChoosing);
        Require(!pause(0), "selection enabled disconnect rule");
        disconnectPause = false;
        teamMode = tag::kVersus;
    }
} // namespace

int main() {
    try {
        RegisterCrossFever();
        RegisterTag();
        for (const char *legacy : {"cross_fever_gauge", "cross_fever_earned_meter"}) {
            for (int old : {0, 1}) {
                std::string key = legacy;
                int value = old;
                TheGame().migrateOption(key, value);
                Require(key == "cross_fever_auto_gauge" && value == 1 - old, "old CLI gauge choice inverted");
            }
        }
        {
            M68K cpu(bus);
            SetRam8(mvsc::kPlaying, 1);
            SetRam8(mvsc::kPlayersIn, 3);
            for (unsigned side = 0; side < 2; ++side) SetRam16(mvsc::kPanel[side], 0x70);
            Require(!hooks.at(0x00BB2C)(cpu), "stock three-punch order change must run");
            for (unsigned side = 0; side < 2; ++side)
                Require(TagPick(mvsc::kPlayer1 + side * mvsc::kPlayerSize) == 1, "second pick should lead before reset");
            Boot();
            for (unsigned side = 0; side < 2; ++side)
                Require(TagPick(mvsc::kPlayer1 + side * mvsc::kPlayerSize) == 0, "warm board boot retained the previous team's starting order");
        }
        TauntOwners();
        for (unsigned intro : {0u, 1u, 6u, 7u, 100u, 180u}) {
            const auto cold = AfterBoot(0, intro);
            for (unsigned prior = 1; prior < 7; prior++) {
                for (unsigned mode = 0; mode < 3; mode++) {
                    Require(AfterBoot(prior, intro, mode != 1, mode != 2) == cold,
                            "reset retained gauge phase " + std::to_string(prior) + " after " + std::to_string(intro) + " intro steps");
                }
            }
        }
        const auto cold = AfterBoot(0, 0);
        ClearOldState();
        enabled = false;
        Credits(20); // outside Cross Fever the clock saturates, rather than wrapping
        Boot();
        enabled = true;
        MakeRecords();
        Require(Credits() == cold, "boot retained the saturated clock from a non-Cross-Fever match");
        for (unsigned phase = 0; phase < 7; phase++) {
            ClearOldState();
            MakeRecords();
            Credits(phase);
            std::vector<uint8_t> bytes;
            Archive save(bytes, true);
            CrossFeverState(save);
            const auto expected = Credits();
            Credits(5);
            Archive load(bytes, false);
            load.version = 11;
            CrossFeverState(load);
            Require(load.Ok() && Credits() == expected, "save/load lost the in-match gauge phase");
        }
        ClearOldState();
        MakeRecords();
        Credits(3);
        memory.fill(0); // a match's records are replaced, without a board reset
        Credits(2);
        MakeRecords();
        Require(!Step() && Step(), "normal match transitions must retain gauge phase");
        for (bool earned : {false, true}) {
            for (bool shorter : {false, true}) {
                ClearOldState();
                MakeRecords();
                gaugeChoice = earned ? 0 : 1;
                arcadeTime = shorter;
                const auto credits = Credits(14);
                Require(std::count(credits.begin(), credits.end(), true) == (earned ? 0 : 2), "meter rule");
                M68K cpu(bus);
                Require(hooks.at(0x015F68)(cpu) == !shorter, "Duo even-step rule");
                SetRam8(mvsc::kStepCount + 1, 1);
                Require(!hooks.at(0x015F68)(cpu), "both timers decrement on odd steps");
            }
        }
        gaugeChoice = 1;
        arcadeTime = false;
        StateVersions();
        // Cross Fever gauges: Only by fighting (issue #363): the mode stays on, the gauges get nothing by themselves
        ClearOldState();
        MakeRecords();
        gaugeChoice = 0;
        for (unsigned n = 0; n < 30; n++) { Require(!Step(), "Only by fighting still filled a gauge"); }
        gaugeChoice = 1;
        Require(Step(), "Fill by themselves did not come back at once"); // (the clock waited at seven meanwhile)
        ReselectRules();
        TeamOrder();
        PartnerTagRequests(0);
        PartnerTagRequests(1);
        DisconnectPause();
        // A saved or CLI setting for MVC must never reorder another game's Tag team.
        TheGame().features.clear();
        tag::Register({});
        teamMode = tag::kVersus;
        teamOrder = 1;
        Require(tag::Person(0, 1) == 2 && tag::Person(1, 0) == 1, "other game adopted MVC team order");
        Require(TheGame().features.size() == 1, "other game advertised an unimplemented team order");
        std::cout << "PASS: Cross Fever reset, clock, team order, ownership and legacy/current marked states\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
