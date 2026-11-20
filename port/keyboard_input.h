// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
#pragma once

#include "bindings.h"
#include "../machine/machine.h"

#include <algorithm>
#include <array>

namespace port {

// The SDL window and its gameplay sampler must agree on who owns each physical Alt key.
class KeyboardInput {
public:
    KeyboardInput(const Bindings &bindings, const BoundTable &table) : bindings(bindings), table(table) {}

    bool Bound(SDL_Scancode key) const {
        for (int player = 0; player < table.table.players; ++player) {
            for (int action = 0; action < table.table.count; ++action) {
                if (bindings.key[player][action] == key) return true;
            }
        }
        return false;
    }

    bool ShortcutAlt(SDL_Keymod mods) const {
        return ((mods & SDL_KMOD_LALT) && !Bound(SDL_SCANCODE_LALT)) ||
               ((mods & SDL_KMOD_RALT) && !Bound(SDL_SCANCODE_RALT));
    }

    enum class Shortcut { None, Fullscreen, Screenshot, Scene, Clip, Record, Replay };
    Shortcut KeyDown(SDL_Scancode key, SDL_Keymod mods, bool repeat) {
        if (repeat || Bound(key)) return Shortcut::None;
        if (key == SDL_SCANCODE_F11) return Shortcut::Fullscreen;
        if (key == SDL_SCANCODE_RETURN) {
            // Remember which action this press started even if Alt comes up before Return.
            fullscreenReturn = ShortcutAlt(mods);
            return fullscreenReturn ? Shortcut::Fullscreen : Shortcut::None;
        }
        if (key != SDL_SCANCODE_F12) return Shortcut::None;
        if (ShortcutAlt(mods)) return (mods & SDL_KMOD_SHIFT) ? Shortcut::Replay : Shortcut::Record;
        if (mods & SDL_KMOD_SHIFT) return Shortcut::Scene;
        if (mods & SDL_KMOD_CTRL) return Shortcut::Clip;
        return Shortcut::Screenshot;
    }

    // The in-game menu also reads Return (Confirm). Keep a fullscreen press out of both
    // consumers through release, including polls where Alt has already come up.
    const bool *Poll(const bool *keys, int count) {
        if (!keys[SDL_SCANCODE_RETURN]) fullscreenReturn = false;
        if (!fullscreenReturn) return keys;
        std::copy_n(keys, std::min(count, static_cast<int>(filtered.size())), filtered.begin());
        filtered[SDL_SCANCODE_RETURN] = false;
        return filtered.data();
    }

    uint16_t Read(const bool *keys, int player, bool zoomHeld, bool captured) const {
        if (captured) return 0;
        uint16_t held = 0;
        for (int action = 0; action < table.table.count; ++action) {
            const SDL_Scancode key = bindings.key[player][action];
            if (key == SDL_SCANCODE_UNKNOWN || !keys[key]) continue;
            if (zoomHeld && (key == SDL_SCANCODE_Q || key == SDL_SCANCODE_E || key == SDL_SCANCODE_W ||
                             key == SDL_SCANCODE_UP || key == SDL_SCANCODE_DOWN || key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_RIGHT)) {
                continue;
            }
            held |= table.bits[action];
        }
        if (player == 0 && keys[SDL_SCANCODE_RETURN] && !Bound(SDL_SCANCODE_RETURN)) {
            held |= cps2recomp::kStart;
        }
        return held;
    }

private:
    const Bindings &bindings;
    const BoundTable &table;
    bool fullscreenReturn = false;
    std::array<bool, SDL_SCANCODE_COUNT> filtered{};
};
} // namespace port
