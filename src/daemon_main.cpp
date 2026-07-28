// Headless OSC control daemon for the RME Fireface mixer.
//
// This is a thin frontend over MixerEngine with no GUI, no meters, and no display
// dependency (links neither ImGui, GLFW, nor OpenGL). Its sole purpose is to expose the
// mixer over OSC, so it forces the OSC endpoint on regardless of the persisted preference.
//
// Lifecycle: parse args -> Init() (service + ALSA) -> start OSC -> timed Tick() loop until
// SIGINT/SIGTERM -> graceful StopOsc(). Any startup failure exits non-zero so a systemd
// unit with Restart=on-failure can own the retry policy (no internal retry loop).

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include "mixer_engine.hpp"

namespace {

// Set from the signal handler only; the main loop polls it. sig_atomic_t keeps the write
// async-signal-safe, and volatile prevents the loop from caching it in a register.
volatile std::sig_atomic_t g_running = 1;

void HandleSignal(int) {
    g_running = 0;
}

void PrintUsage(const char* argv0) {
    std::cout <<
        "Usage: " << argv0 << " [options]\n"
        "\n"
        "Headless OSC control daemon for the RME Fireface mixer.\n"
        "OSC is always enabled in daemon mode (the preferences.json 'enabled' flag is ignored).\n"
        "\n"
        "Options:\n"
        "  --osc-in <port>    UDP port to listen on for control messages (default: preferences.json)\n"
        "  --osc-out <port>   UDP port to send state feedback to on the client host (default: preferences.json)\n"
        "  --card <index>     ALSA card index to bind (default: auto-select first Fireface)\n"
        "  -h, --help         Show this help and exit\n"
        "\n"
        "Note: the OSC endpoint is unauthenticated UDP; run only on a trusted LAN.\n";
}

// Parse a required integer value following a flag. Returns false (and prints an error) if the
// value is missing or not a valid integer.
bool ParseIntArg(int argc, char** argv, int& i, const char* flag, int& out) {
    if (i + 1 >= argc) {
        std::cerr << "Error: " << flag << " requires a value\n";
        return false;
    }
    const char* val = argv[++i];
    char* end = nullptr;
    long parsed = std::strtol(val, &end, 10);
    if (end == val || *end != '\0') {
        std::cerr << "Error: " << flag << " expects an integer, got '" << val << "'\n";
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    int card_index = -1;      // -1 = auto-select first Fireface
    int osc_in_override = -1;  // -1 = keep preferences.json value
    int osc_out_override = -1;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        } else if (std::strcmp(arg, "--osc-in") == 0) {
            if (!ParseIntArg(argc, argv, i, "--osc-in", osc_in_override)) return 2;
        } else if (std::strcmp(arg, "--osc-out") == 0) {
            if (!ParseIntArg(argc, argv, i, "--osc-out", osc_out_override)) return 2;
        } else if (std::strcmp(arg, "--card") == 0) {
            if (!ParseIntArg(argc, argv, i, "--card", card_index)) return 2;
        } else {
            std::cerr << "Error: unknown argument '" << arg << "'\n";
            PrintUsage(argv[0]);
            return 2;
        }
    }

    TotalMixer::MixerEngine engine;

    // Daemon mode exists to serve OSC, so force it on and apply any port overrides before Init.
    TotalMixer::OscPreferences& osc = engine.oscPrefs();
    osc.enabled = true;
    if (osc_in_override >= 0) osc.in_port = osc_in_override;
    if (osc_out_override >= 0) osc.out_port = osc_out_override;

    // Connect to the kernel service and the card. Any failure is fatal (systemd owns retries).
    TotalMixer::MixerEngine::InitResult init = engine.Init(card_index);
    if (!init.connected) {
        std::cerr << "Daemon: failed to connect to the Fireface (service or hardware "
                     "unavailable). Exiting." << std::endl;
        return 1;
    }

    // Bind the OSC sockets. RestartOscServer starts per osc.enabled (forced true above).
    engine.RestartOscServer();
    if (!engine.oscRunning()) {
        std::cerr << "Daemon: failed to start the OSC server on ports in=" << osc.in_port
                  << " out=" << osc.out_port << ". Exiting." << std::endl;
        return 1;
    }

    // Install signal handlers only after we are fully up, so a signal during startup uses the
    // default disposition rather than flipping g_running before the loop begins.
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    std::cout << "Daemon: ready. OSC listening on port " << osc.in_port
              << ", feedback to port " << osc.out_port << ". Press Ctrl+C to stop." << std::endl;

    // Timed service loop. There is no frame clock here, so Tick's internal throttles (500ms
    // poll, 50ms feedback) are driven by an explicit ~5ms sleep. inputs_busy is always false
    // (no widgets to drag). Meters are never polled (no display consumer).
    while (g_running) {
        engine.Tick(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::cout << "\nDaemon: shutting down." << std::endl;
    engine.StopOsc();
    return 0;
}
