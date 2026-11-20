// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Save states: one description of a piece of state serves both to write it and to read it back.
//
//   void Thing::Serialize(Archive &ar) { ar.Pod(m_counter); ar.Vec(m_bytes); }
//
// A state is only read back by the build that wrote it (the header carries the game and a version; the
// sizes of everything written must be the same).
#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace cps2recomp {

class Archive {
public:
    Archive(std::vector<uint8_t> &data, bool saving)
        : m_data(data)
        , m_saving(saving) {}

    // The version of the save state being read, where the port knows it (0: as this build writes): a part added
    // since is read only from states that have it
    uint32_t version = 0;
    // The two v13 writers used different layouts. Machine::Serialize identifies upstream's board DMA
    // at its section boundary, before the game reads its own part of the same archive.
    bool v13BoardDma = false;

    bool Saving() const {
        return m_saving;
    }
    // False once something read did not fit what was written
    bool Ok() const {
        return m_ok;
    }
    bool AtEnd() const {
        return !m_saving && m_at == m_data.size();
    }
    void Check(bool valid) {
        m_ok = m_ok && valid;
    }
    // The sections following machine state in v13 start with four uppercase ASCII letters or digits.
    // Inspect only the current boundary. The receiving section still checks its exact mark.
    bool NextSectionMark() const {
        if (m_saving || m_data.size() - m_at < sizeof(uint32_t)) {
            return false;
        }
        for (size_t i = 0; i < sizeof(uint32_t); ++i) {
            const uint8_t c = m_data[m_at + i];
            if (!(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9')) {
                return false;
            }
        }
        return true;
    }
    // Some preview builds shared a version number with upstream but added different marked parts. Look without
    // consuming bytes, so the game's compatibility path can distinguish an absent part from a required one.
    bool NextMark(uint32_t mark) const {
        if (m_saving || m_at > m_data.size() || m_data.size() - m_at < sizeof(mark)) {
            return false;
        }
        uint32_t value;
        memcpy(&value, m_data.data() + m_at, sizeof(value));
        return value == mark;
    }
    void Bytes(void *p, size_t size) {
        if (!m_ok || size == 0) {
            return;
        }
        if (m_saving) {
            const uint8_t *bytes = static_cast<const uint8_t *>(p);
            m_data.insert(m_data.end(), bytes, bytes + size);
        } else if (size <= m_data.size() - m_at) {
            memcpy(p, &m_data[m_at], size);
            m_at += size;
        } else {
            m_ok = false;
        }
    }
    // A plain value or structure (no pointers into what is not saved)
    template <typename T>
    void Pod(T &value) {
        static_assert(std::is_trivially_copyable_v<T>);
        Bytes(&value, sizeof(T));
    }
    template <typename T>
    void Vec(std::vector<T> &values) {
        static_assert(std::is_trivially_copyable_v<T>);
        uint64_t count = values.size();
        Pod(count);
        if (!m_saving) {
            if (!m_ok || count > (m_data.size() - m_at) / sizeof(T)) {
                m_ok = false;
                return;
            }
            values.resize(static_cast<size_t>(count));
        }
        Bytes(values.data(), static_cast<size_t>(count) * sizeof(T));
    }
    // A mark between parts: reading stops making sense if it is not where it was written
    void Mark(uint32_t mark) {
        uint32_t value = mark;
        Pod(value);
        Check(value == mark);
    }

private:
    std::vector<uint8_t> &m_data;
    size_t m_at = 0;
    bool m_saving, m_ok = true;
};

} // namespace cps2recomp
