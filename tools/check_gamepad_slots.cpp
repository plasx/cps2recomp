// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 cps2recomp contributors. Part of cps2recomp.
// Exercise the frontend's actual slot allocator with four SDL virtual controllers.
#include "../port/gamepad_slots.h"
#include <SDL3/SDL.h>

#include <array>
#include <cstdio>
#include <stdexcept>

namespace {
void Check(bool ok, const char *what) {
    if (!ok) throw std::runtime_error(std::string(what) + " (" + SDL_GetError() + ")");
}

const std::array buttons{SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST,
                         SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER};

SDL_JoystickID Attach(port::GamepadSlots &slots, int model) {
    SDL_VirtualJoystickDesc desc{};
    SDL_INIT_INTERFACE(&desc);
    desc.name = "cps2recomp slot test";
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.vendor_id = 0x1209;
    desc.product_id = static_cast<Uint16>(model + 1);
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    const auto id = SDL_AttachVirtualJoystick(&desc);
    Check(id != 0 && SDL_IsGamepad(id), "attach virtual gamepad");
    slots.Added(id);
    slots.Added(id); // duplicate add notification must not allocate another seat
    return id;
}

void Detach(port::GamepadSlots &slots, SDL_JoystickID id, int seat) {
    Check(SDL_DetachVirtualJoystick(id), "detach virtual gamepad");
    Check(slots.Removed(id) == seat, "reported wrong disconnected seat");
    Check(slots.Removed(id) == -1, "duplicate removal reported a player");
}
} // namespace

int main() {
    port::GamepadSlots slots;
    int status = 0;
    try {
        Check(SDL_Init(SDL_INIT_GAMEPAD), "start SDL");
        std::array<SDL_JoystickID, 4> ids{};
        for (int i = 0; i < 4; ++i) ids[i] = Attach(slots, i);
        Check(slots.Pads().size() == 4, "one physical device per seat");
        for (int i = 0; i < 4; ++i)
            Check(slots.Pads()[i] && SDL_GetGamepadID(slots.Pads()[i]) == ids[i], "initial assignment");
        // Keep the last player's input held as two predecessors disappear.
        SDL_Gamepad *last = slots.Pads()[3];
        SDL_Joystick *joy = SDL_GetGamepadJoystick(last);
        for (auto button : buttons) {
            // Virtual button indices use the descriptor's supported SDL button order.
            Check(SDL_SetJoystickVirtualButton(joy, button, true), "set virtual pad button");
            SDL_UpdateGamepads();
            Check(SDL_GetGamepadButton(last, button), "read each physical button");
        }
        Detach(slots, ids[0], 0);
        Detach(slots, ids[2], 2);
        Check(!slots.Pads()[0] && !slots.Pads()[2] && slots.Pads()[3] == last &&
              SDL_GetGamepadID(slots.Pads()[1]) == ids[1], "disconnect reassigned another player");
        for (auto button : buttons)
            Check(SDL_GetGamepadButton(slots.Pads()[3], button) && !SDL_GetGamepadButton(slots.Pads()[1], button),
                  "disconnect transferred or lost held button");
        // New SDL instance IDs, reverse connection order, same two identifiable models.
        ids[2] = Attach(slots, 2);
        ids[0] = Attach(slots, 0);
        Check(slots.Pads().size() == 4, "reconnected pads allocated extra seats");
        for (int i = 0; i < 4; ++i)
            Check(SDL_GetGamepadID(slots.Pads()[i]) == ids[i], "returning device lost its seat");
        Check(slots.SetEnabled(false), "enter keyboard-only mode");
        for (int i = 0; i < 4; ++i) {
            slots.Added(ids[i]);
            Check(!slots.Pads()[i] && !slots.Has(ids[i]), "keyboard-only mode accepted a native pad");
        }
        Detach(slots, ids[2], -1);
        ids[2] = Attach(slots, 2);
        Check(!slots.Has(ids[2]) && !slots.Pads()[2], "keyboard-only mode opened a hot-plugged pad");
        Check(slots.SetEnabled(true), "restore gamepads");
        for (int i = 0; i < 4; ++i)
            Check(slots.Has(ids[i]) && SDL_GetGamepadID(slots.Pads()[i]) == ids[i], "restoring gamepads changed seats");
        for (int i = 0; i < 4; ++i) Detach(slots, ids[i], i);
        std::puts("PASS: four SDL gamepads, simultaneous buttons, empty seats, reverse-order reconnect, keyboard-only hotplug");
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        status = 1;
    }
    slots.Close();
    SDL_Quit();
    return status;
}
