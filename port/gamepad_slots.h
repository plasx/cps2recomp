// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
#pragma once

#include <SDL3/SDL_gamepad.h>

#include <algorithm>
#include <string>
#include <vector>

namespace port {

// Keep player numbers stable while a game is running. A missing pad leaves a neutral slot.
// Prefer its former slot on reconnect. Identical devices that expose no unique serial or
// path share a model key and take the first matching empty slot.
class GamepadSlots {
public:
    const std::vector<SDL_Gamepad *> &Pads() const { return pads; }

    bool Has(SDL_JoystickID id) const {
        return id && std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    bool SetEnabled(bool on) {
        if (enabled == on) return false;
        enabled = on;
        if (on) {
            int count = 0;
            SDL_JoystickID *connected = SDL_GetGamepads(&count);
            for (int i = 0; i < count; ++i) Added(connected[i]);
            SDL_free(connected);
        } else {
            // Leave the slot keys in place for controllers that are still connected when re-enabled.
            for (SDL_JoystickID id : ids) if (id) Removed(id);
        }
        return true;
    }

    void Added(SDL_JoystickID id) {
        if (!enabled || Has(id)) return;
        SDL_Gamepad *pad = SDL_OpenGamepad(id);
        if (!pad) return;
        char guid[33];
        SDL_GUIDToString(SDL_GetGamepadGUIDForID(id), guid, sizeof(guid));
        std::string key = guid;
        const char *serial = SDL_GetGamepadSerial(pad);
        const char *path = SDL_GetGamepadPath(pad);
        if (serial && *serial) key += std::string(":serial:") + serial;
        else if (path && *path) key += std::string(":path:") + path;
        size_t slot = pads.size();
        for (size_t n = 0; n < pads.size(); ++n) {
            if (pads[n]) continue;
            if (slot == pads.size()) slot = n;
            if (keys[n] == key) { slot = n; break; }
        }
        if (slot == pads.size()) {
            pads.push_back(pad);
            ids.push_back(id);
            keys.push_back(key);
        } else {
            pads[slot] = pad;
            ids[slot] = id;
            keys[slot] = key;
        }
    }

    // The disconnected seat, or -1 for a duplicate/unknown removal event.
    int Removed(SDL_JoystickID id) {
        const auto found = std::find(ids.begin(), ids.end(), id);
        if (!id || found == ids.end()) return -1;
        const size_t slot = static_cast<size_t>(found - ids.begin());
        SDL_CloseGamepad(pads[slot]);
        pads[slot] = nullptr;
        ids[slot] = 0;
        return static_cast<int>(slot);
    }

    void Close() {
        for (SDL_Gamepad *pad : pads) if (pad) SDL_CloseGamepad(pad);
        pads.clear();
        ids.clear();
        keys.clear();
    }

private:
    bool enabled = true;
    std::vector<SDL_Gamepad *> pads;
    std::vector<SDL_JoystickID> ids;
    std::vector<std::string> keys;
};

} // namespace port
