// ╔══════════════════════════════════════════════════════════════════════╗
// ║  DeMoDOOM — Main Entry Point                                       ║
// ║                                                                    ║
// ║  Usage:  demodoom [OPTIONS] [dsp_file]                             ║
// ║                                                                    ║
// ║  Options:                                                          ║
// ║    -r, --rate <Hz>          Sample rate (default: 48000)           ║
// ║    -b, --block <frames>     Block size  (default: 256)             ║
// ║    -R, --resolution <0-5>   Framebuffer resolution preset          ║
// ║    -f, --fullscreen         Start fullscreen                       ║
// ║    --bci                    Enable OpenBCI/LSL                     ║
// ║    -h, --help               Show help                              ║
// ╚══════════════════════════════════════════════════════════════════════╝

#include "core/engine.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

static void print_banner() {
    fprintf(stderr,
        "\033[36m"
        "  ____       __  __       ____   ___   ___  __  __\n"
        " |  _ \\  ___|  \\/  | ___ |  _ \\ / _ \\ / _ \\|  \\/  |\n"
        " | | | |/ _ \\ |\\/| |/ _ \\| | | | | | | | | | |\\/| |\n"
        " | |_| |  __/ |  | | (_) | |_| | |_| | |_| | |  | |\n"
        " |____/ \\___|_|  |_|\\___/|____/ \\___/ \\___/|_|  |_|\n"
        "\033[35m"
        "  DOOM-style Faust DSP Controller  v0.2.0\n"
        "  (c) DeMoD LLC — GPL-3.0\n"
        "\033[0m\n"
    );
}

static void print_help(const char* argv0) {
    print_banner();
    fprintf(stderr,
        "Usage: %s [OPTIONS] [dsp_file]\n\n"
        "Options:\n"
        "  -r, --rate <Hz>          Sample rate (default: 48000)\n"
        "  -b, --block <frames>     Block size  (default: 256)\n"
        "  -R, --resolution <0-%d>   Framebuffer preset:\n",
        argv0, demod::NUM_RESOLUTIONS - 1);

    for (int i = 0; i < demod::NUM_RESOLUTIONS; ++i) {
        fprintf(stderr, "                             %d = %s%s\n",
                i, demod::RESOLUTIONS[i].label,
                i == demod::DEFAULT_RES_IDX ? " [default]" : "");
    }

    fprintf(stderr,
        "  -f, --fullscreen         Start in fullscreen\n"
        "  --bci                    Enable OpenBCI/LSL brain input\n"
        "  -h, --help               Show this help\n"
        "\n"
        "DSP file formats:\n"
        "  .dsp    Faust source        (requires libfaust)\n"
        "  .cpp    Faust C++ output    (compiled at runtime)\n"
        "  .so     Precompiled module  (dlopen)\n"
        "\n"
        "No DSP file = demo mode with built-in test synth.\n"
        "\n"
        "In-app controls:\n"
        "  Esc          Open/close main menu\n"
        "  Tab          Cycle screens (FX Chain / Params / Viz / Settings)\n"
        "  WASD/Arrows  Navigate + adjust parameters\n"
        "  F3           Debug overlay\n"
        "  F11          Toggle fullscreen\n"
        "  Q            Quit\n"
    );
}

int main(int argc, char* argv[]) {
    demod::EngineConfig config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { print_help(argv[0]); return 0; }
        else if ((arg == "-r" || arg == "--rate") && i+1 < argc)
            config.sample_rate = std::atoi(argv[++i]);
        else if ((arg == "-b" || arg == "--block") && i+1 < argc)
            config.block_size = std::atoi(argv[++i]);
        else if ((arg == "-R" || arg == "--resolution") && i+1 < argc)
            config.resolution = std::clamp(std::atoi(argv[++i]), 0, demod::NUM_RESOLUTIONS-1);
        else if (arg == "-f" || arg == "--fullscreen")
            config.fullscreen = true;
        else if (arg == "--bci")
            config.enable_bci = true;
        else if (arg[0] != '-')
            config.dsp_path = arg;
        else {
            fprintf(stderr, "Unknown option: %s\n", arg.c_str());
            print_help(argv[0]); return 1;
        }
    }

    print_banner();
    fprintf(stderr, "  DSP:  %s\n", config.dsp_path.empty() ? "[demo mode]" : config.dsp_path.c_str());
    fprintf(stderr, "  Res:  %s (#%d)\n", demod::RESOLUTIONS[config.resolution].label, config.resolution);
    fprintf(stderr, "  Rate: %d Hz  Block: %d  BCI: %s\n\n",
            config.sample_rate, config.block_size,
            config.enable_bci ? "enabled" : "off");

    demod::Engine engine;
    if (!engine.init(config)) {
        fprintf(stderr, "FATAL: Engine initialization failed.\n");
        return 1;
    }
    engine.run();
    return 0;
}
