#pragma once
// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — Preset System                                          ║
// ║  Save/load FX chain state in multiple formats.                     ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "core/config.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace demod {

enum class PresetFormat : int {
    JSON = 0,
    KEY_VALUE_TEXT,
    BINARY_BLOB,
    REVERSE_TRIE_DB,
    FORMAT_COUNT
};

inline const char* preset_format_name(PresetFormat f) {
    constexpr const char* N[] = {
        "JSON", "Key=Value", "Binary", "Trie DB"
    };
    int i = int(f);
    return (i >= 0 && i < int(PresetFormat::FORMAT_COUNT)) ? N[i] : "???";
}

inline const char* preset_format_ext(PresetFormat f) {
    constexpr const char* E[] = {
        ".json", ".cfg", ".bin", ".rtdb"
    };
    int i = int(f);
    return (i >= 0 && i < int(PresetFormat::FORMAT_COUNT)) ? E[i] : ".bin";
}

struct Preset {
    std::string name;

    struct SlotState {
        std::string dsp_path;
        bool bypassed = true;
        float wet_mix = 1.0f;
        std::unordered_map<std::string, float> params;  // path → value
    };
    std::array<SlotState, MAX_FX_SLOTS> slots;

    // Post-FX renderer state
    struct PostFX {
        bool scanlines = true;
        float scanline_intensity = 0.25f;
        bool bloom = true;
        float bloom_intensity = 0.50f;
        bool vignette = true;
        bool barrel = true;
    } post_fx;
};

class PresetManager {
public:
    PresetManager();

    void set_preset_dir(const std::string& dir);
    std::string preset_dir() const { return preset_dir_; }

    // Get list of preset files in the preset directory
    std::vector<std::string> list_presets() const;

    // Snapshot current state into a Preset
    Preset snapshot(const std::string& name) const;

    // Apply a Preset to the engine
    void apply(const Preset& preset);

    // Save/Load
    bool save(const Preset& preset, PresetFormat format) const;
    bool load(const std::string& filename, Preset& out) const;

    // Set/get current format
    void set_format(PresetFormat f) { format_ = f; }
    PresetFormat format() const { return format_; }

private:
    std::string preset_dir_;
    PresetFormat format_ = PresetFormat::JSON;

    // Format-specific implementations
    bool save_json(const Preset& preset, const std::string& path) const;
    bool load_json(const std::string& path, Preset& out) const;

    bool save_kv_text(const Preset& preset, const std::string& path) const;
    bool load_kv_text(const std::string& path, Preset& out) const;

    bool save_binary(const Preset& preset, const std::string& path) const;
    bool load_binary(const std::string& path, Preset& out) const;

    bool save_reverse_trie(const Preset& preset, const std::string& path) const;
    bool load_reverse_trie(const std::string& path, Preset& out) const;
};

} // namespace demod
