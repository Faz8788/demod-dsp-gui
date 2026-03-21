#pragma once
// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — Chiptune Synthesizer                                   ║
// ║                                                                    ║
// ║  Self-contained 8-bit style music engine for menu/intro.           ║
// ║  Three oscillator voices + noise drum channel:                     ║
// ║    Voice 0: Lead   — square wave, arpeggio melody                  ║
// ║    Voice 1: Bass   — triangle wave, root note pattern              ║
// ║    Voice 2: Pad    — 25% pulse wave, chord stabs                   ║
// ║    Voice 3: Drums  — noise + pitch envelope (kick/hat/snare)       ║
// ║                                                                    ║
// ║  Randomized major/minor arpeggio progressions each restart.        ║
// ║  All DSP is Faust-style: per-sample, no allocation in process().   ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include <cstdint>
#include <atomic>

namespace demod::audio {

class ChiptuneSynth {
public:
    ChiptuneSynth();

    void init(int sample_rate);

    // Reset song to beginning with a new random progression
    void reset();

    // Generate n_frames of interleaved float output (n_channels stride).
    // Adds to the buffer (does not clear it first).
    void process(float* out, int n_channels, int n_frames);

    // Master volume (0.0–1.0)
    void set_volume(float v) { volume_.store(v, std::memory_order_relaxed); }
    float volume() const { return volume_.load(std::memory_order_relaxed); }

    // Enable/disable music globally (persists across menu open/close)
    void set_enabled(bool e) { enabled_.store(e, std::memory_order_relaxed); }
    bool enabled() const { return enabled_.load(std::memory_order_relaxed); }

    // Start/stop playback (only works if enabled)
    void set_playing(bool p) {
        if (p && !enabled_.load(std::memory_order_relaxed)) return;
        playing_.store(p, std::memory_order_relaxed);
    }
    bool playing() const { return playing_.load(std::memory_order_relaxed); }

private:
    int sample_rate_ = 48000;
    float inv_sr_    = 1.0f / 48000.0f;

    std::atomic<float> volume_{0.35f};
    std::atomic<bool>  enabled_{true};
    std::atomic<bool>  playing_{false};

    // ── Song state ───────────────────────────────────────────────────
    // Tick = one 16th note. BPM drives tick length in samples.
    float bpm_         = 140.0f;
    int   samples_per_tick_ = 0;
    int   tick_counter_ = 0;     // Counts down samples to next tick
    int   tick_pos_     = 0;     // Current tick in the pattern (0–63)
    int   pattern_len_  = 64;    // Ticks per pattern loop

    // Chord progression: 4 chords × root MIDI note
    int   progression_[4] = {};
    bool  is_minor_       = false;
    int   base_octave_    = 4;

    // Arpeggio pattern for lead voice (relative semitones from chord root)
    int   arp_pattern_[16] = {};
    int   arp_len_         = 16;

    // ── Voice state ──────────────────────────────────────────────────
    struct ToneVoice {
        float phase     = 0;
        float freq      = 0;
        float env       = 0;
        float env_decay = 0.998f;    // Per-sample envelope decay
        float duty      = 0.5f;     // Pulse width
        enum class Wave { SQUARE, TRIANGLE, PULSE25, SAW } wave = Wave::SQUARE;
    };

    ToneVoice lead_;
    ToneVoice bass_;
    ToneVoice pad_;

    // Noise voice for drums
    struct NoiseVoice {
        uint32_t lfsr    = 0x1234ABCD;
        float    env     = 0;
        float    env_decay = 0.995f;
        float    pitch_env = 0;       // Pitch sweep for kick
        float    pitch_decay = 0.99f;
        float    hp_state = 0;        // Simple highpass for hats
        enum class Type { KICK, SNARE, HAT } type = Type::KICK;
    };

    NoiseVoice drums_;

    // ── Sequencer ────────────────────────────────────────────────────
    // Drum pattern: bit flags per tick position
    static constexpr int KICK_PATTERN  = 0x1;
    static constexpr int SNARE_PATTERN = 0x2;
    static constexpr int HAT_PATTERN   = 0x4;
    uint8_t drum_pattern_[64] = {};

    // ── Helpers ──────────────────────────────────────────────────────
    float midi_to_freq(int note) const;
    float oscillator(ToneVoice& v) const;
    float noise_sample(NoiseVoice& v);
    void  advance_tick();
    void  generate_progression();
    void  generate_arp_pattern();
    void  generate_drum_pattern();
    int   chord_note(int chord_idx, int degree) const;

    // RNG (xorshift32, no stdlib)
    uint32_t rng_state_ = 0;
    uint32_t rng();
    int      rng_range(int lo, int hi); // inclusive
};

} // namespace demod::audio
