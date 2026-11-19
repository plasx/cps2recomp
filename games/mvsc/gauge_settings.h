// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp.
#pragma once

#include "../../recomp-common/settings.h"

namespace mvsc {
inline constexpr char kAutoGauge[] = "cross_fever_auto_gauge";
inline constexpr char kReleasedGauge[] = "cross_fever_gauge", kPreviewGauge[] = "cross_fever_earned_meter";

inline void MigrateGaugeOption(std::string &name, int &value) {
    if (name == kReleasedGauge || name == kPreviewGauge) {
        name = kAutoGauge;
        value = value == 0 ? 1 : 0;
    }
}

// Earlier releases numbered both choices the other way around. Keep a saved choice, including an explicit
// zero, and retire the old keys so resetting a preset cannot revive them. The released key wins over the preview.
inline void MigrateGaugeSettings(recomp::Ini &ini) {
    const std::string stable = ini.get("features", kReleasedGauge), older = ini.get("features", kPreviewGauge);
    if (stable.empty() && older.empty()) return;
    if (ini.get("features", kAutoGauge).empty()) {
        ini.set("features", kAutoGauge, recomp::SettingOff(stable.empty() ? older : stable) ? "1" : "0");
    }
    if (!stable.empty()) ini.set("features", kReleasedGauge, "");
    if (!older.empty()) ini.set("features", kPreviewGauge, "");
    ini.save();
}
} // namespace mvsc
