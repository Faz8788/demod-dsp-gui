// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — Preset Manager Implementation                          ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "core/preset.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

#ifdef HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace demod {

namespace fs = std::filesystem;

PresetManager::PresetManager() {
    // Default preset dir: ~/.config/demodoom/presets/
    const char* home = std::getenv("HOME");
    if (home) {
        preset_dir_ = std::string(home) + "/.config/demodoom/presets";
    } else {
        preset_dir_ = "/tmp/demodoom_presets";
    }
}

void PresetManager::set_preset_dir(const std::string& dir) {
    preset_dir_ = dir;
}

std::vector<std::string> PresetManager::list_presets() const {
    std::vector<std::string> result;
    try {
        if (!fs::exists(preset_dir_)) return result;
        for (const auto& entry : fs::directory_iterator(preset_dir_)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".json" || ext == ".cfg" ||
                    ext == ".bin" || ext == ".rtdb") {
                    result.push_back(entry.path().stem().string());
                }
            }
        }
    } catch (...) {}
    std::sort(result.begin(), result.end());
    return result;
}

Preset PresetManager::snapshot(const std::string& name) const {
    Preset p;
    p.name = name;
    // Slot state and params are captured by the caller via FXChainProcessor
    return p;
}

void PresetManager::apply(const Preset& preset) {
    // Application logic is in engine.cpp which has access to FXChainProcessor
    // This method is a no-op here — the engine calls apply directly
    (void)preset;
}

bool PresetManager::save(const Preset& preset, PresetFormat format) const {
    // Ensure directory exists
    try {
        fs::create_directories(preset_dir_);
    } catch (...) {}

    std::string path = preset_dir_ + "/" + preset.name + preset_format_ext(format);

    switch (format) {
    case PresetFormat::JSON:          return save_json(preset, path);
    case PresetFormat::KEY_VALUE_TEXT: return save_kv_text(preset, path);
    case PresetFormat::BINARY_BLOB:   return save_binary(preset, path);
    case PresetFormat::REVERSE_TRIE_DB: return save_reverse_trie(preset, path);
    default: return false;
    }
}

bool PresetManager::load(const std::string& filename, Preset& out) const {
    // Try to auto-detect format from extension
    auto dot = filename.rfind('.');
    std::string ext = (dot != std::string::npos) ? filename.substr(dot) : "";

    std::string path = filename;
    if (!fs::exists(path)) {
        // Try with preset dir prefix
        path = preset_dir_ + "/" + filename;
    }
    // Try with each extension
    if (!fs::exists(path)) {
        for (int i = 0; i < int(PresetFormat::FORMAT_COUNT); ++i) {
            std::string try_path = preset_dir_ + "/" + filename +
                                   preset_format_ext(PresetFormat(i));
            if (fs::exists(try_path)) { path = try_path; break; }
        }
    }

    if (!fs::exists(path)) {
        fprintf(stderr, "[PRESET] Not found: %s\n", filename.c_str());
        return false;
    }

    if (path.size() >= 5 && path.substr(path.size()-5) == ".json")
        return load_json(path, out);
    if (path.size() >= 4 && path.substr(path.size()-4) == ".cfg")
        return load_kv_text(path, out);
    if (path.size() >= 4 && path.substr(path.size()-4) == ".bin")
        return load_binary(path, out);
    if (path.size() >= 5 && path.substr(path.size()-5) == ".rtdb")
        return load_reverse_trie(path, out);

    return false;
}

// ═══════════════════════════════════════════════════════════════════════
//  JSON FORMAT (nlohmann-json)
// ═══════════════════════════════════════════════════════════════════════

bool PresetManager::save_json(const Preset& preset, const std::string& path) const {
#ifdef HAVE_NLOHMANN_JSON
    nlohmann::json j;
    j["name"] = preset.name;
    j["version"] = 1;

    auto& slots = j["slots"];
    for (int i = 0; i < MAX_FX_SLOTS; ++i) {
        const auto& s = preset.slots[i];
        nlohmann::json slot;
        slot["dsp_path"] = s.dsp_path;
        slot["bypassed"] = s.bypassed;
        slot["wet_mix"]  = s.wet_mix;
        auto& params = slot["params"];
        for (const auto& [k, v] : s.params) {
            params[k] = v;
        }
        slots.push_back(slot);
    }

    j["post_fx"]["scanlines"] = preset.post_fx.scanlines;
    j["post_fx"]["scanline_intensity"] = preset.post_fx.scanline_intensity;
    j["post_fx"]["bloom"] = preset.post_fx.bloom;
    j["post_fx"]["bloom_intensity"] = preset.post_fx.bloom_intensity;
    j["post_fx"]["vignette"] = preset.post_fx.vignette;
    j["post_fx"]["barrel"] = preset.post_fx.barrel;

    std::ofstream f(path);
    if (!f) return false;
    f << j.dump(2);
    return true;
#else
    (void)preset; (void)path;
    fprintf(stderr, "[PRESET] JSON support not compiled in\n");
    return false;
#endif
}

bool PresetManager::load_json(const std::string& path, Preset& out) const {
#ifdef HAVE_NLOHMANN_JSON
    std::ifstream f(path);
    if (!f) return false;

    nlohmann::json j;
    try { f >> j; } catch (...) { return false; }

    out.name = j.value("name", "");

    if (j.contains("slots")) {
        const auto& slots = j["slots"];
        for (int i = 0; i < std::min((int)slots.size(), MAX_FX_SLOTS); ++i) {
            const auto& s = slots[i];
            out.slots[i].dsp_path = s.value("dsp_path", "");
            out.slots[i].bypassed = s.value("bypassed", true);
            out.slots[i].wet_mix  = s.value("wet_mix", 1.0f);
            if (s.contains("params")) {
                for (auto& [k, v] : s["params"].items()) {
                    out.slots[i].params[k] = v.get<float>();
                }
            }
        }
    }

    if (j.contains("post_fx")) {
        const auto& pf = j["post_fx"];
        out.post_fx.scanlines = pf.value("scanlines", true);
        out.post_fx.scanline_intensity = pf.value("scanline_intensity", 0.25f);
        out.post_fx.bloom = pf.value("bloom", true);
        out.post_fx.bloom_intensity = pf.value("bloom_intensity", 0.50f);
        out.post_fx.vignette = pf.value("vignette", true);
        out.post_fx.barrel = pf.value("barrel", true);
    }

    return true;
#else
    (void)path; (void)out;
    return false;
#endif
}

// ═══════════════════════════════════════════════════════════════════════
//  KEY=VALUE TEXT FORMAT
// ═══════════════════════════════════════════════════════════════════════

bool PresetManager::save_kv_text(const Preset& preset, const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;

    f << "# DeMoDOOM Preset: " << preset.name << "\n";
    f << "name=" << preset.name << "\n\n";

    for (int i = 0; i < MAX_FX_SLOTS; ++i) {
        const auto& s = preset.slots[i];
        std::string prefix = "slot" + std::to_string(i) + ".";
        f << prefix << "dsp_path=" << s.dsp_path << "\n";
        f << prefix << "bypassed=" << (s.bypassed ? "1" : "0") << "\n";
        f << prefix << "wet_mix=" << s.wet_mix << "\n";
        for (const auto& [k, v] : s.params) {
            f << prefix << "param." << k << "=" << v << "\n";
        }
        f << "\n";
    }

    f << "post_fx.scanlines=" << (preset.post_fx.scanlines ? "1" : "0") << "\n";
    f << "post_fx.scanline_intensity=" << preset.post_fx.scanline_intensity << "\n";
    f << "post_fx.bloom=" << (preset.post_fx.bloom ? "1" : "0") << "\n";
    f << "post_fx.bloom_intensity=" << preset.post_fx.bloom_intensity << "\n";
    f << "post_fx.vignette=" << (preset.post_fx.vignette ? "1" : "0") << "\n";
    f << "post_fx.barrel=" << (preset.post_fx.barrel ? "1" : "0") << "\n";

    return true;
}

bool PresetManager::load_kv_text(const std::string& path, Preset& out) const {
    std::ifstream f(path);
    if (!f) return false;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "name") {
            out.name = val;
        } else if (key.substr(0, 4) == "slot") {
            auto dot = key.find('.');
            if (dot == std::string::npos) continue;
            int slot_idx = std::stoi(key.substr(4, dot - 4));
            std::string field = key.substr(dot + 1);

            if (slot_idx < 0 || slot_idx >= MAX_FX_SLOTS) continue;
            auto& s = out.slots[slot_idx];

            if (field == "dsp_path") s.dsp_path = val;
            else if (field == "bypassed") s.bypassed = (val == "1");
            else if (field == "wet_mix") s.wet_mix = std::stof(val);
            else if (field.substr(0, 6) == "param.") {
                s.params[field.substr(6)] = std::stof(val);
            }
        } else if (key == "post_fx.scanlines") out.post_fx.scanlines = (val == "1");
        else if (key == "post_fx.scanline_intensity") out.post_fx.scanline_intensity = std::stof(val);
        else if (key == "post_fx.bloom") out.post_fx.bloom = (val == "1");
        else if (key == "post_fx.bloom_intensity") out.post_fx.bloom_intensity = std::stof(val);
        else if (key == "post_fx.vignette") out.post_fx.vignette = (val == "1");
        else if (key == "post_fx.barrel") out.post_fx.barrel = (val == "1");
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════════
//  BINARY BLOB FORMAT
// ═══════════════════════════════════════════════════════════════════════

// Header
#pragma pack(push, 1)
struct PresetBinHeader {
    uint32_t magic;       // 'DMDP'
    uint32_t version;     // 1
    char     name[64];
    uint8_t  num_slots;
    uint8_t  post_fx_flags;  // bit0=scanlines, bit1=bloom, bit2=vignette, bit3=barrel
    float    scanline_intensity;
    float    bloom_intensity;
    uint32_t data_offset; // offset to slot data
};
#pragma pack(pop)

static constexpr uint32_t PRESET_MAGIC = 0x504D4444; // 'DMDP'

bool PresetManager::save_binary(const Preset& preset, const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    PresetBinHeader hdr{};
    hdr.magic = PRESET_MAGIC;
    hdr.version = 1;
    strncpy(hdr.name, preset.name.c_str(), sizeof(hdr.name)-1);
    hdr.num_slots = MAX_FX_SLOTS;
    hdr.post_fx_flags =
        (preset.post_fx.scanlines ? 1 : 0) |
        (preset.post_fx.bloom ? 2 : 0) |
        (preset.post_fx.vignette ? 4 : 0) |
        (preset.post_fx.barrel ? 8 : 0);
    hdr.scanline_intensity = preset.post_fx.scanline_intensity;
    hdr.bloom_intensity = preset.post_fx.bloom_intensity;
    hdr.data_offset = sizeof(PresetBinHeader);

    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    for (int i = 0; i < MAX_FX_SLOTS; ++i) {
        const auto& s = preset.slots[i];
        uint8_t bypassed = s.bypassed ? 1 : 0;
        uint16_t path_len = (uint16_t)s.dsp_path.size();
        uint16_t param_count = (uint16_t)s.params.size();

        f.write(reinterpret_cast<const char*>(&bypassed), 1);
        f.write(reinterpret_cast<const char*>(&s.wet_mix), 4);
        f.write(reinterpret_cast<const char*>(&path_len), 2);
        f.write(s.dsp_path.data(), path_len);
        f.write(reinterpret_cast<const char*>(&param_count), 2);

        for (const auto& [k, v] : s.params) {
            uint16_t klen = (uint16_t)k.size();
            f.write(reinterpret_cast<const char*>(&klen), 2);
            f.write(k.data(), klen);
            f.write(reinterpret_cast<const char*>(&v), 4);
        }
    }

    return f.good();
}

bool PresetManager::load_binary(const std::string& path, Preset& out) const {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    PresetBinHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (hdr.magic != PRESET_MAGIC || hdr.version != 1) return false;

    out.name = hdr.name;
    out.post_fx.scanlines = (hdr.post_fx_flags & 1) != 0;
    out.post_fx.bloom = (hdr.post_fx_flags & 2) != 0;
    out.post_fx.vignette = (hdr.post_fx_flags & 4) != 0;
    out.post_fx.barrel = (hdr.post_fx_flags & 8) != 0;
    out.post_fx.scanline_intensity = hdr.scanline_intensity;
    out.post_fx.bloom_intensity = hdr.bloom_intensity;

    for (int i = 0; i < std::min((int)hdr.num_slots, MAX_FX_SLOTS); ++i) {
        auto& s = out.slots[i];
        uint8_t bypassed;
        uint16_t path_len, param_count;

        f.read(reinterpret_cast<char*>(&bypassed), 1);
        f.read(reinterpret_cast<char*>(&s.wet_mix), 4);
        f.read(reinterpret_cast<char*>(&path_len), 2);
        s.dsp_path.resize(path_len);
        f.read(&s.dsp_path[0], path_len);
        s.bypassed = bypassed != 0;

        f.read(reinterpret_cast<char*>(&param_count), 2);
        for (int j = 0; j < param_count; ++j) {
            uint16_t klen;
            f.read(reinterpret_cast<char*>(&klen), 2);
            std::string key(klen, '\0');
            f.read(&key[0], klen);
            float val;
            f.read(reinterpret_cast<char*>(&val), 4);
            s.params[key] = val;
        }
    }

    return f.good();
}

// ═══════════════════════════════════════════════════════════════════════
//  REVERSE TRIE KEY-VALUE DATABASE FORMAT
// ═══════════════════════════════════════════════════════════════════════

// Format:
//   Header: magic(4) + version(4) + name(64) + num_entries(4)
//   Index:  [num_entries] of { key_offset(4), val_offset(4), key_len(2), val_len(2) }
//   Keys:   concatenated reversed key strings
//   Values: concatenated float values (4 bytes each)
// Keys are stored reversed (e.g., "/synth/cutoff" → "tuocC/htnys/")
// Index is sorted by reversed key for binary search.

struct RTDBHeader {
    uint32_t magic;       // 'RTDB'
    uint32_t version;     // 1
    char     name[64];
    uint32_t num_entries;
};

struct RTDBEntry {
    uint32_t key_offset;
    uint32_t val_offset;
    uint16_t key_len;
    uint16_t val_len;
};

static constexpr uint32_t RTDB_MAGIC = 0x42445452; // 'RTDB'

static std::string reverse_string(const std::string& s) {
    std::string r(s.rbegin(), s.rend());
    return r;
}

bool PresetManager::save_reverse_trie(const Preset& preset, const std::string& path) const {
    // Build flat key-value list from preset
    struct KV { std::string key; float value; };
    std::vector<KV> entries;

    for (int i = 0; i < MAX_FX_SLOTS; ++i) {
        const auto& s = preset.slots[i];
        std::string prefix = "slot" + std::to_string(i);

        entries.push_back({ prefix + ".bypassed", s.bypassed ? 1.0f : 0.0f });
        entries.push_back({ prefix + ".wet_mix", s.wet_mix });

        for (const auto& [k, v] : s.params) {
            entries.push_back({ prefix + ".param." + k, v });
        }
    }

    entries.push_back({ "post_fx.scanlines", preset.post_fx.scanlines ? 1.0f : 0.0f });
    entries.push_back({ "post_fx.scanline_intensity", preset.post_fx.scanline_intensity });
    entries.push_back({ "post_fx.bloom", preset.post_fx.bloom ? 1.0f : 0.0f });
    entries.push_back({ "post_fx.bloom_intensity", preset.post_fx.bloom_intensity });
    entries.push_back({ "post_fx.vignette", preset.post_fx.vignette ? 1.0f : 0.0f });
    entries.push_back({ "post_fx.barrel", preset.post_fx.barrel ? 1.0f : 0.0f });

    // Reverse-sort by reversed key
    std::sort(entries.begin(), entries.end(), [](const KV& a, const KV& b) {
        return reverse_string(a.key) < reverse_string(b.key);
    });

    // Build binary layout
    uint32_t num = (uint32_t)entries.size();

    // Compute offsets
    uint32_t header_size = sizeof(RTDBHeader);
    uint32_t index_size = num * sizeof(RTDBEntry);
    uint32_t keys_offset = header_size + index_size;

    // Compute key offsets
    uint32_t running_key_offset = 0;
    std::vector<uint32_t> key_offsets(num);
    for (uint32_t i = 0; i < num; ++i) {
        key_offsets[i] = running_key_offset;
        running_key_offset += (uint32_t)entries[i].key.size();
    }

    // Write
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    RTDBHeader hdr{};
    hdr.magic = RTDB_MAGIC;
    hdr.version = 1;
    strncpy(hdr.name, preset.name.c_str(), sizeof(hdr.name)-1);
    hdr.num_entries = num;
    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    // Write index
    for (uint32_t i = 0; i < num; ++i) {
        RTDBEntry entry{};
        entry.key_offset = keys_offset + key_offsets[i];
        entry.val_offset = 0; // computed below
        entry.key_len = (uint16_t)entries[i].key.size();
        entry.val_len = 4;
        f.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
    }

    // Write keys
    for (const auto& e : entries) {
        f.write(e.key.data(), e.key.size());
    }

    // Write values (float)
    for (const auto& e : entries) {
        f.write(reinterpret_cast<const char*>(&e.value), 4);
    }

    // Patch val_offset in index (seek back)
    uint32_t vals_offset = keys_offset + running_key_offset;
    for (uint32_t i = 0; i < num; ++i) {
        uint32_t entry_pos = header_size + i * sizeof(RTDBEntry) + 4; // +4 = offset of val_offset field
        f.seekp(entry_pos);
        uint32_t voff = vals_offset + i * 4;
        f.write(reinterpret_cast<const char*>(&voff), 4);
    }

    return f.good();
}

bool PresetManager::load_reverse_trie(const std::string& path, Preset& out) const {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    RTDBHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (hdr.magic != RTDB_MAGIC || hdr.version != 1) return false;

    out.name = hdr.name;
    uint32_t num = hdr.num_entries;

    // Read index
    std::vector<RTDBEntry> index(num);
    for (uint32_t i = 0; i < num; ++i) {
        f.read(reinterpret_cast<char*>(&index[i]), sizeof(RTDBEntry));
    }

    // Read each key-value pair
    for (uint32_t i = 0; i < num; ++i) {
        const auto& e = index[i];

        // Read key
        f.seekg(e.key_offset);
        std::string key(e.key_len, '\0');
        f.read(&key[0], e.key_len);

        // Read value
        f.seekg(e.val_offset);
        float val = 0;
        f.read(reinterpret_cast<char*>(&val), 4);

        // Parse key into preset fields
        if (key.substr(0, 4) == "slot") {
            auto dot = key.find('.');
            if (dot == std::string::npos) continue;
            int slot_idx = std::stoi(key.substr(4, dot - 4));
            std::string field = key.substr(dot + 1);
            if (slot_idx < 0 || slot_idx >= MAX_FX_SLOTS) continue;

            auto& s = out.slots[slot_idx];
            if (field == "bypassed") s.bypassed = (val != 0.0f);
            else if (field == "wet_mix") s.wet_mix = val;
            else if (field.substr(0, 6) == "param.") {
                s.params[field.substr(6)] = val;
            }
        } else if (key == "post_fx.scanlines") out.post_fx.scanlines = (val != 0.0f);
        else if (key == "post_fx.scanline_intensity") out.post_fx.scanline_intensity = val;
        else if (key == "post_fx.bloom") out.post_fx.bloom = (val != 0.0f);
        else if (key == "post_fx.bloom_intensity") out.post_fx.bloom_intensity = val;
        else if (key == "post_fx.vignette") out.post_fx.vignette = (val != 0.0f);
        else if (key == "post_fx.barrel") out.post_fx.barrel = (val != 0.0f);
    }

    return true;
}

} // namespace demod
