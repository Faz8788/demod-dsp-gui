// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — Chiptune Synthesizer Implementation                    ║
// ║  Pure C++ DSP — zero allocations in process(), RT-safe.            ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "audio/chiptune.hpp"
#include <cmath>
#include <cstring>

namespace demod::audio {

static constexpr float PI  = 3.14159265358979f;
static constexpr float TAU = 6.28318530717959f;

// ── Major and minor scale intervals from root ────────────────────────
// Used to build chord tones: root, 3rd, 5th, 7th
static constexpr int MAJOR_TRIAD[] = { 0, 4, 7 };         // root, M3, P5
static constexpr int MINOR_TRIAD[] = { 0, 3, 7 };         // root, m3, P5
static constexpr int MAJOR_7TH[]   = { 0, 4, 7, 11 };     // root, M3, P5, M7
static constexpr int MINOR_7TH[]   = { 0, 3, 7, 10 };     // root, m3, P5, m7

// Common chord progressions (as scale degrees 0–6)
// Major: I–V–vi–IV, I–IV–V–V, I–vi–IV–V
// Minor: i–iv–VII–III, i–VI–III–VII, i–iv–v–i
static constexpr int MAJOR_PROGS[][4] = {
    {0, 4, 5, 3},   // I  V  vi IV
    {0, 3, 4, 4},   // I  IV V  V
    {0, 5, 3, 4},   // I  vi IV V
    {0, 4, 3, 4},   // I  V  IV V
    {0, 3, 0, 4},   // I  IV I  V
};
static constexpr int MINOR_PROGS[][4] = {
    {0, 3, 6, 2},   // i   iv  VII III
    {0, 5, 2, 6},   // i   VI  III VII
    {0, 3, 4, 0},   // i   iv  v   i
    {0, 6, 5, 4},   // i   VII VI  v
    {0, 2, 3, 6},   // i   III iv  VII
};
static constexpr int NUM_MAJOR_PROGS = sizeof(MAJOR_PROGS) / sizeof(MAJOR_PROGS[0]);
static constexpr int NUM_MINOR_PROGS = sizeof(MINOR_PROGS) / sizeof(MINOR_PROGS[0]);

// Major scale semitone offsets from root
static constexpr int MAJOR_SCALE[] = { 0, 2, 4, 5, 7, 9, 11 };
// Natural minor scale
static constexpr int MINOR_SCALE[] = { 0, 2, 3, 5, 7, 8, 10 };

ChiptuneSynth::ChiptuneSynth() {
    // Seed RNG from address bits (no stdlib random in RT code)
    rng_state_ = uint32_t(uintptr_t(this) ^ 0xDEADBEEF);
    // Ensure non-zero
    if (rng_state_ == 0) rng_state_ = 0x12345678;
}

void ChiptuneSynth::init(int sample_rate) {
    sample_rate_ = sample_rate;
    inv_sr_ = 1.0f / float(sample_rate);
    samples_per_tick_ = int(float(sample_rate) * 60.0f / (bpm_ * 4.0f));

    lead_.wave = ToneVoice::Wave::SQUARE;
    lead_.duty = 0.5f;
    lead_.env_decay = 0.9997f;

    bass_.wave = ToneVoice::Wave::TRIANGLE;
    bass_.env_decay = 0.9999f;

    pad_.wave = ToneVoice::Wave::PULSE25;
    pad_.duty = 0.25f;
    pad_.env_decay = 0.99995f;

    reset();
}

void ChiptuneSynth::reset() {
    tick_counter_ = 0;
    tick_pos_ = 0;

    lead_.phase = 0; lead_.env = 0;
    bass_.phase = 0; bass_.env = 0;
    pad_.phase  = 0; pad_.env  = 0;
    drums_.env  = 0;

    // Randomize everything
    bpm_ = float(rng_range(120, 160));
    samples_per_tick_ = int(float(sample_rate_) * 60.0f / (bpm_ * 4.0f));

    is_minor_ = (rng() & 1) != 0;
    base_octave_ = rng_range(3, 5);

    generate_progression();
    generate_arp_pattern();
    generate_drum_pattern();
}

// ═══════════════════════════════════════════════════════════════════════
//  GENERATION
// ═══════════════════════════════════════════════════════════════════════

void ChiptuneSynth::generate_progression() {
    // Pick a random progression template
    const int* prog;
    const int* scale;
    if (is_minor_) {
        prog  = MINOR_PROGS[rng_range(0, NUM_MINOR_PROGS - 1)];
        scale = MINOR_SCALE;
    } else {
        prog  = MAJOR_PROGS[rng_range(0, NUM_MAJOR_PROGS - 1)];
        scale = MAJOR_SCALE;
    }

    // Pick a random root note (C=0 through B=11)
    int root = rng_range(0, 11);

    for (int i = 0; i < 4; ++i) {
        int degree = prog[i] % 7;
        progression_[i] = root + scale[degree] + base_octave_ * 12;
    }
}

void ChiptuneSynth::generate_arp_pattern() {
    // Build a 16-step arpeggio pattern of chord degrees
    // Mix of root, 3rd, 5th, octave with rhythmic variation
    const int* triad = is_minor_ ? MINOR_TRIAD : MAJOR_TRIAD;

    // Several arp styles
    int style = rng_range(0, 4);

    switch (style) {
    case 0: // Rising triad + octave
        for (int i = 0; i < 16; ++i) {
            int deg = i % 4;
            arp_pattern_[i] = (deg < 3) ? triad[deg] : 12;
        }
        break;
    case 1: // Falling triad + octave
        for (int i = 0; i < 16; ++i) {
            int seq[] = {12, 7, is_minor_ ? 3 : 4, 0};
            arp_pattern_[i] = seq[i % 4];
        }
        break;
    case 2: // Alternating root-fifth
        for (int i = 0; i < 16; ++i) {
            arp_pattern_[i] = (i % 2 == 0) ? 0 : 7;
        }
        break;
    case 3: // Full chord strum up then down
        {
            int up[]   = {0, triad[1], triad[2], 12, triad[2], triad[1], 0, 0};
            for (int i = 0; i < 16; ++i)
                arp_pattern_[i] = up[i % 8];
        }
        break;
    case 4: // Random chord tones
        for (int i = 0; i < 16; ++i) {
            int opts[] = {0, triad[1], triad[2], 12, 12 + triad[1]};
            arp_pattern_[i] = opts[rng_range(0, 4)];
        }
        break;
    }

    // Randomly punch holes (rests) in ~25% of positions
    for (int i = 0; i < 16; ++i) {
        if (rng_range(0, 3) == 0)
            arp_pattern_[i] = -1; // -1 = rest
    }
    // Always play beat 0
    if (arp_pattern_[0] == -1)
        arp_pattern_[0] = 0;
}

void ChiptuneSynth::generate_drum_pattern() {
    std::memset(drum_pattern_, 0, sizeof(drum_pattern_));

    // Kick on beats
    int kick_style = rng_range(0, 2);
    for (int i = 0; i < 64; ++i) {
        switch (kick_style) {
        case 0: // Four on the floor
            if (i % 4 == 0) drum_pattern_[i] |= KICK_PATTERN;
            break;
        case 1: // Boom-bap
            if (i % 16 == 0 || i % 16 == 6 || i % 16 == 10)
                drum_pattern_[i] |= KICK_PATTERN;
            break;
        case 2: // Sparse
            if (i % 8 == 0) drum_pattern_[i] |= KICK_PATTERN;
            break;
        }
    }

    // Snare on backbeats
    int snare_style = rng_range(0, 2);
    for (int i = 0; i < 64; ++i) {
        switch (snare_style) {
        case 0: // Standard backbeat
            if (i % 8 == 4) drum_pattern_[i] |= SNARE_PATTERN;
            break;
        case 1: // Double-time
            if (i % 4 == 2) drum_pattern_[i] |= SNARE_PATTERN;
            break;
        case 2: // Syncopated
            if (i % 16 == 4 || i % 16 == 10 || i % 16 == 14)
                drum_pattern_[i] |= SNARE_PATTERN;
            break;
        }
    }

    // Hihats — 8ths or 16ths
    bool sixteenths = (rng() & 1) != 0;
    for (int i = 0; i < 64; ++i) {
        if (sixteenths) {
            drum_pattern_[i] |= HAT_PATTERN;
        } else {
            if (i % 2 == 0) drum_pattern_[i] |= HAT_PATTERN;
        }
    }

    // Random ghost notes
    for (int i = 0; i < 64; ++i) {
        if (rng_range(0, 7) == 0 && !(drum_pattern_[i] & HAT_PATTERN))
            drum_pattern_[i] |= HAT_PATTERN;
    }
}

int ChiptuneSynth::chord_note(int chord_idx, int degree) const {
    // Get the MIDI note for a given chord tone degree
    int root = progression_[chord_idx % 4];
    const int* triad = is_minor_ ? MINOR_TRIAD : MAJOR_TRIAD;
    if (degree >= 0 && degree < 3)
        return root + triad[degree];
    return root + degree; // Fallback: semitone offset
}

// ═══════════════════════════════════════════════════════════════════════
//  PER-SAMPLE DSP
// ═══════════════════════════════════════════════════════════════════════

float ChiptuneSynth::midi_to_freq(int note) const {
    return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
}

float ChiptuneSynth::oscillator(ToneVoice& v) const {
    float out = 0;
    switch (v.wave) {
    case ToneVoice::Wave::SQUARE:
        out = (v.phase < v.duty) ? 1.0f : -1.0f;
        break;
    case ToneVoice::Wave::TRIANGLE:
        out = (v.phase < 0.5f)
            ? ( 4.0f * v.phase - 1.0f)
            : (-4.0f * v.phase + 3.0f);
        break;
    case ToneVoice::Wave::PULSE25:
        out = (v.phase < v.duty) ? 1.0f : -1.0f;
        break;
    case ToneVoice::Wave::SAW:
        out = 2.0f * v.phase - 1.0f;
        break;
    }
    return out;
}

float ChiptuneSynth::noise_sample(NoiseVoice& v) {
    // Galois LFSR (maximal length 32-bit)
    uint32_t bit = ((v.lfsr >> 0) ^ (v.lfsr >> 2) ^
                    (v.lfsr >> 6) ^ (v.lfsr >> 7)) & 1;
    v.lfsr = (v.lfsr >> 1) | (bit << 31);
    return (v.lfsr & 1) ? 1.0f : -1.0f;
}

// ═══════════════════════════════════════════════════════════════════════
//  SEQUENCER
// ═══════════════════════════════════════════════════════════════════════

void ChiptuneSynth::advance_tick() {
    int chord_idx = (tick_pos_ / 16) % 4;  // 16 ticks per chord = 1 bar
    int sub_tick  = tick_pos_ % 16;

    // ── Lead: arpeggio ───────────────────────────────────────────────
    int arp_note = arp_pattern_[sub_tick % arp_len_];
    if (arp_note >= 0) {
        int midi = progression_[chord_idx] + arp_note + 12; // One octave up
        lead_.freq = midi_to_freq(midi);
        lead_.env  = 0.8f;
        // Slight duty cycle modulation per note for character
        lead_.duty = 0.4f + 0.1f * float(sub_tick % 4);
    }

    // ── Bass: root note, half notes ──────────────────────────────────
    if (sub_tick % 8 == 0) {
        int bass_note = progression_[chord_idx] - 12; // One octave down
        bass_.freq = midi_to_freq(bass_note);
        bass_.env  = 0.9f;
    }
    // Bass can do a quick octave slide on beat 3
    if (sub_tick == 12) {
        int bass_note = progression_[chord_idx] - 12 + 7; // Fifth
        bass_.freq = midi_to_freq(bass_note);
        bass_.env  = 0.6f;
    }

    // ── Pad: chord stabs on strong beats ─────────────────────────────
    if (sub_tick == 0 || sub_tick == 8) {
        // Play the 3rd of the chord
        int pad_note = chord_note(chord_idx, 1); // 3rd
        pad_.freq = midi_to_freq(pad_note);
        pad_.env  = 0.5f;
    }

    // ── Drums ────────────────────────────────────────────────────────
    uint8_t drum_flags = drum_pattern_[tick_pos_ % 64];

    if (drum_flags & KICK_PATTERN) {
        drums_.type = NoiseVoice::Type::KICK;
        drums_.env  = 1.0f;
        drums_.env_decay = 0.997f;
        drums_.pitch_env = 200.0f;   // Start high, sweep down
        drums_.pitch_decay = 0.985f;
    }
    if (drum_flags & SNARE_PATTERN) {
        drums_.type = NoiseVoice::Type::SNARE;
        drums_.env  = 0.7f;
        drums_.env_decay = 0.996f;
        drums_.pitch_env = 0;
    }
    if (drum_flags & HAT_PATTERN) {
        // Only trigger hat if not already playing a louder drum
        if (drums_.env < 0.3f) {
            drums_.type = NoiseVoice::Type::HAT;
            drums_.env  = 0.35f;
            drums_.env_decay = 0.992f;
            drums_.pitch_env = 0;
        }
    }

    // Advance
    tick_pos_ = (tick_pos_ + 1) % pattern_len_;
}

// ═══════════════════════════════════════════════════════════════════════
//  MAIN PROCESS
// ═══════════════════════════════════════════════════════════════════════

void ChiptuneSynth::process(float* out, int n_channels, int n_frames) {
    if (!playing_.load(std::memory_order_relaxed)) return;
    float vol = volume_.load(std::memory_order_relaxed);
    if (vol <= 0.0001f) return;

    for (int i = 0; i < n_frames; ++i) {
        // ── Sequencer tick ───────────────────────────────────────────
        if (--tick_counter_ <= 0) {
            tick_counter_ = samples_per_tick_;
            advance_tick();
        }

        // ── Lead oscillator ──────────────────────────────────────────
        float lead_out = oscillator(lead_) * lead_.env * 0.18f;
        lead_.phase += lead_.freq * inv_sr_;
        if (lead_.phase >= 1.0f) lead_.phase -= 1.0f;
        lead_.env *= lead_.env_decay;

        // ── Bass oscillator ──────────────────────────────────────────
        float bass_out = oscillator(bass_) * bass_.env * 0.22f;
        bass_.phase += bass_.freq * inv_sr_;
        if (bass_.phase >= 1.0f) bass_.phase -= 1.0f;
        bass_.env *= bass_.env_decay;

        // ── Pad oscillator ───────────────────────────────────────────
        float pad_out = oscillator(pad_) * pad_.env * 0.10f;
        pad_.phase += pad_.freq * inv_sr_;
        if (pad_.phase >= 1.0f) pad_.phase -= 1.0f;
        pad_.env *= pad_.env_decay;

        // ── Drums ────────────────────────────────────────────────────
        float drum_out = 0;
        float ns = noise_sample(drums_);

        switch (drums_.type) {
        case NoiseVoice::Type::KICK: {
            // Sine wave with pitch sweep + a bit of noise
            float kick_freq = 50.0f + drums_.pitch_env;
            float kick_sine = std::sin(drums_.pitch_env * 0.05f +
                              kick_freq * inv_sr_ * TAU);
            drum_out = (kick_sine * 0.7f + ns * 0.15f) * drums_.env * 0.30f;
            drums_.pitch_env *= drums_.pitch_decay;
        } break;

        case NoiseVoice::Type::SNARE: {
            // Noise + resonant body
            float body = std::sin(180.0f * inv_sr_ * TAU * drums_.env);
            drum_out = (ns * 0.5f + body * 0.3f) * drums_.env * 0.22f;
        } break;

        case NoiseVoice::Type::HAT: {
            // High-passed noise
            float raw = ns * drums_.env * 0.15f;
            drum_out = raw - drums_.hp_state;
            drums_.hp_state += (raw - drums_.hp_state) * 0.15f;
        } break;
        }

        drums_.env *= drums_.env_decay;
        if (drums_.env < 0.001f) drums_.env = 0;

        // ── Mix ──────────────────────────────────────────────────────
        float mono = (lead_out + bass_out + pad_out + drum_out) * vol;

        // Soft clip (tanh approximation: x / (1 + |x|))
        float clipped = mono / (1.0f + std::fabs(mono));

        // Slight stereo: lead slightly left, pad slightly right
        float left  = clipped + lead_out * vol * 0.05f;
        float right = clipped + pad_out  * vol * 0.05f;

        int base = i * n_channels;
        out[base + 0] += left;
        if (n_channels >= 2)
            out[base + 1] += right;
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  RNG
// ═══════════════════════════════════════════════════════════════════════

uint32_t ChiptuneSynth::rng() {
    // xorshift32
    uint32_t x = rng_state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state_ = x;
    return x;
}

int ChiptuneSynth::rng_range(int lo, int hi) {
    if (lo >= hi) return lo;
    return lo + int(rng() % uint32_t(hi - lo + 1));
}

} // namespace demod::audio
