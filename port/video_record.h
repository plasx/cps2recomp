// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// Recording what is played as a video (issue #376): the game's pictures and its sound, the custom soundtrack's with
// it, in an MP4 file that plays anywhere (H.264 and AAC). The pictures are made larger, each of the board's pixels a
// sharp block, in the picture's shape as the Display page has it; the file's time is the game's, a frame of it for
// each of the board's frames run (stopped, nothing is recorded; slowed or sped up, it plays at the board's speed).
//
// Encoding is done on a thread of its own, so the game goes on as it does: what is handed over is the board's
// picture as it is, small, and the sound. Should that thread fall behind (a slow machine at a large size), pictures
// are left out, each one before shown for longer, and the sound is all kept: the video is then less smooth, not late.
//
// minih264 makes the H.264 (third_party/minih264e.h), FAAC the AAC (third_party/faac/), and minimp4 writes the file
// (third_party/minimp4.h).
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct SDL_AudioStream;

namespace port {

class VideoRecorder {
public:
    VideoRecorder();
    VideoRecorder(const VideoRecorder &) = delete;
    VideoRecorder &operator=(const VideoRecorder &) = delete;
    ~VideoRecorder(); // (finishes the file being written)

    // Starts writing `path`: pictures `width` x `height` (even numbers), at `fps` of the board's frames a second;
    // false, with why in `error`, if it cannot be written
    bool Start(const std::string &path, int width, int height, double fps, std::string &error);
    bool Recording() const {
        return m_running;
    }
    // One of the game's pictures (`columns` x `rows`, 0xAABBGGRR as the board's are), shown for `frames` of its frames
    // and `shape` times as wide as it is tall (it is fitted in the video's size, black round it if its shape is not
    // the video's)
    void Picture(const uint32_t *pixels, int columns, int rows, float shape, int frames);
    // The sound made since the last picture: the game's, two channels of 16 bits at `rate`; and the custom
    // soundtrack's, two channels of floats at its own rate (both mixed, as they are heard)
    void Sound(const int16_t *samples, size_t frames, int rate);
    void Music(const float *samples, size_t frames, int rate);
    // Ends the file; false if it could not all be written
    bool Stop();
    // The video's length so far, in seconds
    double Seconds() const {
        return m_fps > 0 ? m_framesIn / m_fps : 0;
    }
    const std::string &Path() const {
        return m_path;
    }
    // Pictures waiting to be encoded: enough that more would be left out soon (the instant replay waits for fewer)
    bool Busy() {
        std::lock_guard<std::mutex> hold(m_lock);
        return m_jobs.size() >= 30;
    }
    // Not playing in real time (headless, a movie made into a video): the game waits for the encoder rather than
    // pictures being left out
    void SetWaiting(bool waiting) {
        m_waiting = waiting;
    }
    // Pictures left out because encoding fell behind
    uint64_t Dropped() const {
        return m_dropped;
    }

    // The video's size for a picture of this shape (width over height): `lines` tall, as wide as the shape asks
    // (made even)
    static void SizeFor(float shape, int lines, int &width, int &height);

private:
    struct Job {
        std::vector<uint32_t> pixels;
        int columns = 0, rows = 0;
        float shape = 1;
        int frames = 0;
        std::vector<int16_t> sound; // (at kRate, two channels)
    };
    static constexpr int kRate = 48000;
    void Run();
    void Encode(Job &job);
    void Mix(std::vector<int16_t> &out, size_t frames);
    bool Finish();

    std::string m_path;
    int m_width = 0, m_height = 0;
    double m_fps = 0;
    bool m_running = false;
    uint64_t m_framesIn = 0;   // (the board's frames handed over)
    uint64_t m_soundOut = 0;   // (the sound's frames handed over, at kRate)
    uint64_t m_dropped = 0;
    size_t m_trim = 0;         // (the sound's frames still to leave out at the start: the AAC encoder's delay)
    // The sound, at kRate: the game's and the soundtrack's, each through a converter from its own rate
    SDL_AudioStream *m_game = nullptr, *m_music = nullptr;
    int m_gameRate = 0, m_musicRate = 0;
    std::vector<float> m_gameBuffer, m_musicBuffer;

    // (the encoding thread's)
    std::thread m_thread;
    std::mutex m_lock;
    std::condition_variable m_wake, m_taken;
    std::deque<Job> m_jobs;
    bool m_ending = false, m_failed = false, m_waiting = false;
    struct State;
    std::unique_ptr<State> m_state;
};

} // namespace port
