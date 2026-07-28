#pragma once

#include <sys/types.h>

namespace TotalMixer {

// Manages the optional Rust web-remote bridge (linux-totalmix-web-remote) as a child process,
// launched from the GUI. The bridge serves a phone-friendly PWA and relays it to this app's
// OSC endpoint. GUI-only on purpose: the headless daemon deliberately does NOT spawn the
// bridge (systemd pairs the two units instead), so nothing here lives in mixer_engine.
//
// The child is discovered on PATH by name; no configurable path (kept minimal for now).
class BridgeProcess {
public:
    BridgeProcess() = default;
    ~BridgeProcess();

    BridgeProcess(const BridgeProcess&) = delete;
    BridgeProcess& operator=(const BridgeProcess&) = delete;

    // Name the GUI execs, and the web port the PWA listens on (for the URLs the GUI shows).
    static constexpr const char* kBinaryName = "linux-totalmix-web-remote";
    static constexpr int kWebPort = 8451;

    // True if kBinaryName is resolvable on PATH. Cheap enough to call each frame.
    static bool IsAvailable();

    // Spawn the bridge relaying to the desktop's OSC ports. osc_in_port is where the desktop
    // listens for commands (bridge --osc-send-port); osc_out_port is where the desktop sends
    // feedback (bridge --osc-recv-port). allow_external binds 0.0.0.0 so a LAN phone can reach
    // it. No-op if already running. Returns false on fork/exec setup failure.
    bool Start(int osc_in_port, int osc_out_port, bool allow_external);

    // Send SIGTERM and reap. Safe to call when not running.
    void Stop();

    // Non-blocking reap: if the child exited on its own, clears running state and records the
    // exit code. Call once per frame to avoid leaving a zombie. Returns running().
    bool Poll();

    bool running() const { return pid_ > 0; }
    pid_t pid() const { return pid_; }
    // Exit code of the last child that terminated, or -1 if none has (or it was signalled).
    int last_exit_code() const { return last_exit_code_; }

private:
    pid_t pid_ = -1;
    int last_exit_code_ = -1;
};

} // namespace TotalMixer
