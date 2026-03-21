// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoD DSP GUI — PipeWire Audio Engine Implementation              ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "audio/audio_engine.hpp"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/utils/result.h>

#include <cstring>
#include <cstdio>
#include <chrono>

namespace demod::audio {

struct AudioEngine::HookData {
    AudioEngine*   engine;
    struct spa_hook listener;
};

static struct pw_stream_events make_stream_events() {
    struct pw_stream_events ev = {};
    ev.version = PW_VERSION_STREAM_EVENTS;
    ev.state_changed = [](void* data, enum pw_stream_state old,
                        enum pw_stream_state state, const char* error) {
        auto* hd = static_cast<AudioEngine::HookData*>(data);
        AudioEngine::on_state_changed(hd, uint32_t(old),
                                       uint32_t(state), error);
    };
    ev.process = [](void* data) {
        auto* hd = static_cast<AudioEngine::HookData*>(data);
        AudioEngine::on_process(hd);
    };
    return ev;
}
static const struct pw_stream_events stream_events = make_stream_events();

AudioEngine::AudioEngine() {
    pw_init(nullptr, nullptr);
}

AudioEngine::~AudioEngine() {
    stop();
    pw_deinit();
}

bool AudioEngine::start(const std::string& client_name) {
    if (running_) return true;

    hook_data_ = new HookData{this, {}};

    // Create PipeWire main loop in its own thread
    loop_ = pw_main_loop_new(nullptr);
    if (!loop_) {
        fprintf(stderr, "[AUDIO] Failed to create PipeWire main loop\n");
        return false;
    }

    auto props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, client_name.c_str(),
        PW_KEY_NODE_NAME, client_name.c_str(),
        PW_KEY_NODE_LATENCY, (std::to_string(block_size_) + "/" +
                              std::to_string(sample_rate_)).c_str(),
        nullptr
    );

    stream_ = pw_stream_new_simple(
        pw_main_loop_get_loop(loop_),
        client_name.c_str(),
        props,
        &stream_events,
        hook_data_
    );

    if (!stream_) {
        fprintf(stderr, "[AUDIO] Failed to create PipeWire stream\n");
        return false;
    }

    // Build audio format params
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    struct spa_audio_info_raw info = {};
    info.format   = SPA_AUDIO_FORMAT_F32;
    info.rate     = (uint32_t)sample_rate_;
    info.channels = (uint32_t)n_outputs_;

    const struct spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    int res = pw_stream_connect(
        stream_,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT |
                          PW_STREAM_FLAG_MAP_BUFFERS |
                          PW_STREAM_FLAG_RT_PROCESS),
        params, 1
    );

    if (res < 0) {
        fprintf(stderr, "[AUDIO] Stream connect failed: %s\n",
                spa_strerror(res));
        return false;
    }

    running_ = true;

    // Run PipeWire loop in dedicated thread
    pw_thread_ = std::thread(&AudioEngine::pw_thread_func, this);

    fprintf(stderr, "[AUDIO] Started: %d Hz, %d ch, %d block\n",
            sample_rate_, n_outputs_, block_size_);
    return true;
}

void AudioEngine::stop() {
    if (!running_) return;
    running_ = false;

    if (loop_) {
        pw_main_loop_quit(loop_);
    }

    if (pw_thread_.joinable()) {
        pw_thread_.join();
    }

    if (stream_) {
        pw_stream_destroy(stream_);
        stream_ = nullptr;
    }
    if (loop_) {
        pw_main_loop_destroy(loop_);
        loop_ = nullptr;
    }
    if (hook_data_) {
        delete hook_data_;
        hook_data_ = nullptr;
    }
}

void AudioEngine::pw_thread_func() {
    pw_main_loop_run(loop_);
}

void AudioEngine::on_process(void* userdata) {
    auto* hd = static_cast<HookData*>(userdata);
    auto* engine = hd->engine;

    struct pw_buffer* pwbuf = pw_stream_dequeue_buffer(engine->stream_);
    if (!pwbuf) return;

    struct spa_buffer* spa_buf = pwbuf->buffer;
    float* dst = nullptr;
    int n_frames = 0;

    if (spa_buf->datas[0].data) {
        dst = static_cast<float*>(spa_buf->datas[0].data);
        n_frames = spa_buf->datas[0].maxsize /
                   (sizeof(float) * engine->n_outputs_);
        if (pwbuf->requested > 0 && (int)pwbuf->requested < n_frames) {
            n_frames = (int)pwbuf->requested;
        }
    }

    if (!dst || n_frames <= 0) {
        pw_stream_queue_buffer(engine->stream_, pwbuf);
        return;
    }

    // Measure processing time for CPU load
    auto t0 = std::chrono::high_resolution_clock::now();

    // Call the DSP callback
    if (engine->callback_) {
        // Interleaved output — we need to deinterleave for Faust
        // For now, provide interleaved pointer and let the bridge handle it
        float* outputs[] = { dst };
        engine->callback_(nullptr, outputs, engine->n_outputs_, n_frames);
    } else {
        // Silence
        std::memset(dst, 0, n_frames * engine->n_outputs_ * sizeof(float));
    }

    // Copy to scope ring buffer (channel 0 only)
    for (int i = 0; i < n_frames; ++i) {
        int pos = engine->scope_write_pos.load(std::memory_order_relaxed);
        engine->scope_buffer[pos % SCOPE_BUF_SIZE] =
            dst[i * engine->n_outputs_]; // Channel 0
        engine->scope_write_pos.store(
            (pos + 1) % SCOPE_BUF_SIZE, std::memory_order_relaxed);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    double budget  = double(n_frames) / engine->sample_rate_;
    engine->cpu_load_.store(float(elapsed / budget),
                            std::memory_order_relaxed);

    spa_buf->datas[0].chunk->offset = 0;
    spa_buf->datas[0].chunk->stride = sizeof(float) * engine->n_outputs_;
    spa_buf->datas[0].chunk->size   = n_frames * engine->n_outputs_ *
                                      sizeof(float);

    pw_stream_queue_buffer(engine->stream_, pwbuf);
}

void AudioEngine::on_state_changed(void* userdata, uint32_t old_state,
                                    uint32_t state, const char* error) {
    (void)old_state;
    auto* hd = static_cast<HookData*>(userdata);

    const char* state_name = pw_stream_state_as_string(
        static_cast<pw_stream_state>(state));

    fprintf(stderr, "[AUDIO] Stream state: %s", state_name);
    if (error) fprintf(stderr, " (error: %s)", error);
    fprintf(stderr, "\n");

    if (static_cast<pw_stream_state>(state) == PW_STREAM_STATE_ERROR) {
        hd->engine->running_ = false;
    }
}

} // namespace demod::audio
