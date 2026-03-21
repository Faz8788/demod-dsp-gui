// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoD DSP GUI — Faust Bridge Implementation                       ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "audio/faust_bridge.hpp"
#include <dlfcn.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <random>
#include <climits>

#ifdef HAVE_LIBFAUST
#include <faust/dsp/llvm-dsp.h>
#include <faust/dsp/dsp.h>
#endif

namespace demod::audio {

// ── FaustParamUI implementation ──────────────────────────────────────

std::string FaustParamUI::make_path(const char* label) const {
    if (current_path_.empty()) return std::string("/") + label;
    return current_path_ + "/" + label;
}

void FaustParamUI::declare(float*, const char*, const char*) {}

void FaustParamUI::openTabBox(const char* label) {
    current_path_ = make_path(label);
}
void FaustParamUI::openHorizontalBox(const char* label) {
    current_path_ = make_path(label);
}
void FaustParamUI::openVerticalBox(const char* label) {
    current_path_ = make_path(label);
}
void FaustParamUI::closeBox() {
    auto pos = current_path_.rfind('/');
    if (pos != std::string::npos && pos > 0) {
        current_path_ = current_path_.substr(0, pos);
    } else {
        current_path_.clear();
    }
}

void FaustParamUI::addButton(const char* label, float* zone) {
    params_.push_back({
        param_index_++, make_path(label), label,
        0.0f, 1.0f, 0.0f, 1.0f,
        ParamDescriptor::Type::BUTTON
    });
    zones_.push_back(zone);
}

void FaustParamUI::addCheckButton(const char* label, float* zone) {
    params_.push_back({
        param_index_++, make_path(label), label,
        0.0f, 1.0f, 0.0f, 1.0f,
        ParamDescriptor::Type::CHECKBOX
    });
    zones_.push_back(zone);
}

void FaustParamUI::addVerticalSlider(const char* label, float* zone,
                                      float init, float min, float max,
                                      float step) {
    params_.push_back({
        param_index_++, make_path(label), label,
        min, max, init, step,
        ParamDescriptor::Type::SLIDER
    });
    zones_.push_back(zone);
}

void FaustParamUI::addHorizontalSlider(const char* label, float* zone,
                                        float init, float min, float max,
                                        float step) {
    params_.push_back({
        param_index_++, make_path(label), label,
        min, max, init, step,
        ParamDescriptor::Type::SLIDER
    });
    zones_.push_back(zone);
}

void FaustParamUI::addNumEntry(const char* label, float* zone,
                                float init, float min, float max,
                                float step) {
    params_.push_back({
        param_index_++, make_path(label), label,
        min, max, init, step,
        ParamDescriptor::Type::NENTRY
    });
    zones_.push_back(zone);
}

void FaustParamUI::addHorizontalBargraph(const char* label, float* zone,
                                          float min, float max) {
    meters_.push_back({make_path(label), zone});
    (void)min; (void)max;
}

void FaustParamUI::addVerticalBargraph(const char* label, float* zone,
                                        float min, float max) {
    meters_.push_back({make_path(label), zone});
    (void)min; (void)max;
}

// ── FaustBridge ──────────────────────────────────────────────────────

FaustBridge::FaustBridge() = default;

FaustBridge::~FaustBridge() {
    unload();
}

bool FaustBridge::load_dsp_source(const std::string& filepath,
                                   int sample_rate) {
#ifdef HAVE_LIBFAUST
    unload();

    std::string error_msg;
    int argc = 0;
    const char* argv[] = { nullptr };

    llvm_dsp_factory* factory = createDSPFactoryFromFile(
        filepath, argc, argv, "", error_msg, -1);

    if (!factory) {
        fprintf(stderr, "[FAUST] JIT compile failed: %s\n",
                error_msg.c_str());
        return false;
    }

    dsp* raw_dsp = factory->createDSPInstance();
    if (!raw_dsp) {
        fprintf(stderr, "[FAUST] Failed to create DSP instance\n");
        deleteDSPFactory(factory);
        return false;
    }

    raw_dsp->init(sample_rate);

    // Discover parameters via UI visitor
    raw_dsp->buildUserInterface(&ui_);
    param_zones_ = ui_.zones();

    // Keep factory and DSP alive for the lifetime of this bridge
    jit_factory_ = factory;
    jit_dsp_     = raw_dsp;

    dsp_name_    = filepath;
    num_inputs_  = raw_dsp->getNumInputs();
    num_outputs_ = raw_dsp->getNumOutputs();

    // Allocate deinterleave buffers
    int max_ch = std::max(num_inputs_, num_outputs_);
    deinterleave_buf_.resize(max_ch);
    for (auto& buf : deinterleave_buf_) {
        buf.resize(AUDIO_BLOCKSIZE * 2, 0.0f);
    }

    fprintf(stderr, "[FAUST] Loaded JIT: %s (%d in, %d out, %d params)\n",
            filepath.c_str(), num_inputs_, num_outputs_, num_params());
    return true;
#else
    (void)filepath; (void)sample_rate;
    fprintf(stderr, "[FAUST] JIT not available (libfaust not linked)\n");
    fprintf(stderr, "[FAUST] Use load_dsp_library() with a pre-compiled .so\n");
    return false;
#endif
}

bool FaustBridge::load_dsp_library(const std::string& so_path,
                                    int sample_rate) {
    unload();

    dl_handle_ = dlopen(so_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!dl_handle_) {
        fprintf(stderr, "[FAUST] dlopen failed: %s\n", dlerror());
        return false;
    }

    // Faust-generated shared libraries export a createDSP() function
    using CreateFunc = FaustDSP* (*)();
    auto create = (CreateFunc)dlsym(dl_handle_, "createDSP");

    if (!create) {
        // Try alternate symbol name
        create = (CreateFunc)dlsym(dl_handle_, "newDsp");
    }

    if (!create) {
        fprintf(stderr, "[FAUST] No createDSP symbol in %s\n",
                so_path.c_str());
        dlclose(dl_handle_);
        dl_handle_ = nullptr;
        return false;
    }

    dsp_ = create();
    if (!dsp_) {
        fprintf(stderr, "[FAUST] createDSP() returned null\n");
        dlclose(dl_handle_);
        dl_handle_ = nullptr;
        return false;
    }

    dsp_->init(sample_rate);
    dsp_->buildUserInterface(&ui_);

    // Cache parameter zone pointers for RT-safe access
    param_zones_ = ui_.zones();

    dsp_name_    = so_path;
    num_inputs_  = dsp_->getNumInputs();
    num_outputs_ = dsp_->getNumOutputs();

    // Allocate deinterleave buffers
    int max_ch = std::max(num_inputs_, num_outputs_);
    deinterleave_buf_.resize(max_ch);
    for (auto& buf : deinterleave_buf_) {
        buf.resize(AUDIO_BLOCKSIZE * 2, 0.0f);
    }

    fprintf(stderr, "[FAUST] Loaded DSO: %s (%d in, %d out, %d params)\n",
            so_path.c_str(), num_inputs_, num_outputs_, num_params());
    return true;
}

bool FaustBridge::load_dsp_cpp(const std::string& cpp_path,
                                int sample_rate) {
    // Compile the C++ file to a shared library, then dlopen it
    std::string so_path = "/tmp/demod_faust_" +
                          std::to_string(getpid()) + ".so";

    // Use fork+execvp to avoid shell interpretation entirely
    fprintf(stderr, "[FAUST] Compiling: c++ -shared -fPIC -O2 -std=c++17 -o %s %s\n",
            so_path.c_str(), cpp_path.c_str());

    pid_t pid = fork();
    if (pid < 0) {
        perror("[FAUST] fork failed");
        return false;
    }

    if (pid == 0) {
        // Child process — exec the compiler directly
        const char* args[] = {
            "c++", "-shared", "-fPIC", "-O2", "-std=c++17",
            "-o", so_path.c_str(), cpp_path.c_str(),
            nullptr
        };
        execvp("c++", const_cast<char**>(args));
        perror("[FAUST] execvp failed");
        _exit(127);
    }

    // Parent — wait for compilation
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("[FAUST] waitpid failed");
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[FAUST] Compilation failed (exit %d)\n",
                WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return false;
    }

    return load_dsp_library(so_path, sample_rate);
}

void FaustBridge::unload() {
    if (dsp_) {
        delete dsp_;
        dsp_ = nullptr;
    }
#ifdef HAVE_LIBFAUST
    if (jit_dsp_) {
        delete static_cast< ::dsp*>(jit_dsp_);
        jit_dsp_ = nullptr;
    }
    if (jit_factory_) {
        deleteDSPFactory(static_cast<llvm_dsp_factory*>(jit_factory_));
        jit_factory_ = nullptr;
    }
#endif
    if (dl_handle_) {
        dlclose(dl_handle_);
        dl_handle_ = nullptr;
    }
    param_zones_.clear();
    ui_ = FaustParamUI{};
    dsp_name_.clear();
    num_inputs_ = num_outputs_ = 0;
}

float FaustBridge::get_param(int index) const {
    std::lock_guard<std::mutex> lock(param_mutex_);
    if (index < 0 || index >= (int)param_zones_.size()) return 0.0f;
    return *param_zones_[index];
}

void FaustBridge::set_param(int index, float value) {
    std::lock_guard<std::mutex> lock(param_mutex_);
    if (index < 0 || index >= (int)param_zones_.size()) return;
    const auto& desc = ui_.params()[index];
    *param_zones_[index] = std::clamp(value, desc.min, desc.max);
}

float FaustBridge::get_param(const std::string& path) const {
    std::lock_guard<std::mutex> lock(param_mutex_);
    for (const auto& p : ui_.params()) {
        if (p.path == path) {
            if (p.index >= 0 && p.index < (int)param_zones_.size())
                return *param_zones_[p.index];
            return 0.0f;
        }
    }
    return 0.0f;
}

void FaustBridge::set_param(const std::string& path, float value) {
    std::lock_guard<std::mutex> lock(param_mutex_);
    for (const auto& p : ui_.params()) {
        if (p.path == path) {
            if (p.index >= 0 && p.index < (int)param_zones_.size())
                *param_zones_[p.index] = std::clamp(value, p.min, p.max);
            return;
        }
    }
}

void FaustBridge::reset_params() {
    std::lock_guard<std::mutex> lock(param_mutex_);
    for (const auto& p : ui_.params()) {
        if (p.index >= 0 && p.index < (int)param_zones_.size())
            *param_zones_[p.index] = p.init;
    }
}

void FaustBridge::randomize_params() {
    std::lock_guard<std::mutex> lock(param_mutex_);
    static std::mt19937 rng(std::random_device{}());
    for (const auto& p : ui_.params()) {
        if (p.type == ParamDescriptor::Type::BUTTON) continue;
        if (p.index < 0 || p.index >= (int)param_zones_.size()) continue;
        std::uniform_real_distribution<float> dist(p.min, p.max);
        // Quantize to step
        float val = dist(rng);
        if (p.step > 0) {
            val = std::round((val - p.min) / p.step) * p.step + p.min;
        }
        *param_zones_[p.index] = std::clamp(val, p.min, p.max);
    }
}

void FaustBridge::process(const float* const* inputs, float* const* outputs,
                           int n_channels, int n_frames) {
    if (!loaded()) {
        // Silence
        for (int ch = 0; ch < n_channels; ++ch) {
            std::memset(outputs[ch], 0, n_frames * sizeof(float));
        }
        return;
    }

    // Faust expects non-interleaved float** for both inputs and outputs
    float* in_ptrs[32]  = {};
    float* out_ptrs[32] = {};

    for (int ch = 0; ch < std::min(num_inputs_, 32); ++ch) {
        in_ptrs[ch] = inputs ? const_cast<float*>(inputs[ch]) : nullptr;
    }
    for (int ch = 0; ch < std::min(num_outputs_, 32); ++ch) {
        out_ptrs[ch] = outputs[ch];
    }

#ifdef HAVE_LIBFAUST
    if (jit_dsp_) {
        static_cast< ::dsp*>(jit_dsp_)->compute(n_frames, in_ptrs, out_ptrs);
        return;
    }
#endif
    if (dsp_) {
        dsp_->compute(n_frames, in_ptrs, out_ptrs);
    }
}

void FaustBridge::process_interleaved(float* interleaved_out, int n_frames) {
    if (!loaded() || num_outputs_ <= 0 || n_frames <= 0) {
        if (n_frames > 0 && num_outputs_ > 0)
            std::memset(interleaved_out, 0,
                        size_t(n_frames) * size_t(num_outputs_) * sizeof(float));
        return;
    }

    // Ensure buffers are large enough
    for (int ch = 0; ch < num_outputs_; ++ch) {
        if ((int)deinterleave_buf_[ch].size() < n_frames) {
            deinterleave_buf_[ch].resize(n_frames, 0.0f);
        }
    }

    float* out_ptrs[32] = {};
    for (int ch = 0; ch < num_outputs_; ++ch) {
        out_ptrs[ch] = deinterleave_buf_[ch].data();
    }

#ifdef HAVE_LIBFAUST
    if (jit_dsp_) {
        static_cast< ::dsp*>(jit_dsp_)->compute(n_frames, nullptr, out_ptrs);
    } else
#endif
    if (dsp_) {
        dsp_->compute(n_frames, nullptr, out_ptrs);
    }

    // Interleave
    for (int i = 0; i < n_frames; ++i) {
        for (int ch = 0; ch < num_outputs_; ++ch) {
            interleaved_out[i * num_outputs_ + ch] =
                deinterleave_buf_[ch][i];
        }
    }
}

void FaustBridge::process_interleaved(const float* const* inputs,
                                       float* interleaved_out, int n_frames) {
    if (!loaded() || num_outputs_ <= 0 || n_frames <= 0) {
        if (n_frames > 0 && num_outputs_ > 0)
            std::memset(interleaved_out, 0,
                        size_t(n_frames) * size_t(num_outputs_) * sizeof(float));
        return;
    }

    // Ensure output deinterleave buffers are large enough
    for (int ch = 0; ch < num_outputs_; ++ch) {
        if ((int)deinterleave_buf_[ch].size() < n_frames) {
            deinterleave_buf_[ch].resize(n_frames, 0.0f);
        }
    }

    // Build non-interleaved input pointers
    float* in_ptrs[32] = {};
    if (inputs) {
        for (int ch = 0; ch < std::min(num_inputs_, 32); ++ch) {
            in_ptrs[ch] = const_cast<float*>(inputs[ch]);
        }
    }

    // Build non-interleaved output pointers
    float* out_ptrs[32] = {};
    for (int ch = 0; ch < num_outputs_; ++ch) {
        out_ptrs[ch] = deinterleave_buf_[ch].data();
    }

#ifdef HAVE_LIBFAUST
    if (jit_dsp_) {
        static_cast< ::dsp*>(jit_dsp_)->compute(n_frames, in_ptrs, out_ptrs);
    } else
#endif
    if (dsp_) {
        dsp_->compute(n_frames, in_ptrs, out_ptrs);
    }

    // Interleave output
    for (int i = 0; i < n_frames; ++i) {
        for (int ch = 0; ch < num_outputs_; ++ch) {
            interleaved_out[i * num_outputs_ + ch] =
                deinterleave_buf_[ch][i];
        }
    }
}

void FaustBridge::add_axis_mapping(Action axis_action, AxisMapping mapping) {
    axis_mappings_[axis_action].push_back(mapping);
}

void FaustBridge::clear_axis_mappings() {
    axis_mappings_.clear();
}

void FaustBridge::apply_axis(Action axis_action, float value) {
    auto it = axis_mappings_.find(axis_action);
    if (it == axis_mappings_.end()) return;

    std::lock_guard<std::mutex> lock(param_mutex_);
    for (const auto& m : it->second) {
        if (m.param_index < 0 || m.param_index >= (int)param_zones_.size())
            continue;
        const auto& desc = ui_.params()[m.param_index];
        float normalized;
        if (m.bidirectional) {
            // 0.5 = center = default
            normalized = (value - 0.5f) * 2.0f; // -1..1
            float center = desc.init;
            float range  = (normalized > 0) ? (desc.max - center)
                                            : (center - desc.min);
            *param_zones_[m.param_index] = std::clamp(
                center + normalized * range, desc.min, desc.max);
        } else {
            // Linear map
            normalized = (value - m.axis_min) /
                         (m.axis_max - m.axis_min);
            normalized = std::clamp(normalized, 0.0f, 1.0f);
            *param_zones_[m.param_index] = std::clamp(
                desc.min + normalized * (desc.max - desc.min),
                desc.min, desc.max);
        }
    }
}

} // namespace demod::audio
