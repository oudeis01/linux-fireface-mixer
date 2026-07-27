#pragma once

#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include "mixer_types.hpp"
#include "alsa_core.hpp"
#include "service_checker.hpp"
#include "osc_server.hpp"

namespace TotalMixer {

// Headless mixer core: owns the ALSA connection, the mixer domain state, the shared apply
// primitives, hardware polling, and the OSC endpoint (inbound apply + diff feedback). It has
// zero dependency on ImGui/GLFW/OpenGL so it can back both the GUI and a headless daemon.
//
// Threading: identical to the original GUI model. All mixer state lives on the caller's thread;
// only the OSC command queue and client address cross threads (inside OscServer). Call Tick()
// from a single thread (the GUI frame loop, or the daemon's timed loop).
class MixerEngine {
public:
    MixerEngine();
    ~MixerEngine();

    MixerEngine(const MixerEngine&) = delete;
    MixerEngine& operator=(const MixerEngine&) = delete;

    // Result of Init(): whether the ALSA card connected, plus the raw service state so the
    // frontend can render an appropriate status (GUI) or decide to exit (daemon).
    struct InitResult {
        bool connected = false;
        ServiceStatus service = ServiceStatus::NotRunning;
    };

    // Check the kernel service, connect to the Fireface card, and take a first hardware poll.
    // Does not start OSC (call StartOsc separately). Safe to call once at startup.
    InitResult Init();

    // ── OSC endpoint ──
    OscPreferences& oscPrefs() { return osc_prefs; }
    const OscPreferences& oscPrefs() const { return osc_prefs; }
    void RestartOscServer();   // (re)start or stop per osc_prefs.enabled
    void StopOsc();

    // One service cycle: drain+apply inbound OSC, throttled hardware poll, throttled diff push.
    // inputs_busy lets the GUI suppress polling while a widget is being dragged; the daemon
    // always passes false. Meter polling is NOT done here (GUI-only, via alsa()).
    void Tick(bool inputs_busy = false);

    // ── Shared apply primitives (single write path for both UI edits and OSC commands) ──
    void SetMasterVolume(int ch, long val);
    void SetMasterMute(int ch, bool mute);
    void SetMasterSolo(int ch, bool solo);
    void SetMasterLink(int ch, bool linked);
    void SetSourceGain(bool is_playback, int src_idx, int output, long val);
    void SetSourceMute(bool is_playback, int src_idx, int output, bool mute);
    void SetSubmix(int output);

    // ── State reads (for GUI render / daemon introspection) ──
    const ChannelState& master(int ch) const { return master_states[ch]; }
    ChannelState& master(int ch) { return master_states[ch]; }
    int selectedOutput() const { return selected_output; }
    long sourceGain(bool is_playback, int output, int src_idx) const;
    bool sourceMuted(bool is_playback, int output, int src_idx) const;
    int OutputLinkPartner(int ch) const;
    bool IsOutputSelected(int ch) const;

    // GUI interaction hint: the crosspoint currently being dragged, which polling must not
    // overwrite. The daemon never sets these (no dragging).
    void SetHeldCrosspoint(int output, int src_idx);
    void ClearHeldCrosspoint();

    // Direct crosspoint write (analog/spdif/adat or stream), used by the primitives and the UI.
    bool WriteSourceGain(bool is_playback, int src_idx, int output, long val);

    // GUI-only concerns (meters, arbitrary Control tab, device info) go through the ALSA handle.
    AlsaCore* alsa() { return alsa_.get(); }
    bool connected() const { return alsa_ != nullptr; }

    // Meter tuning is persisted alongside OSC prefs, so the engine owns it after config load.
    MeterPreferences& meterPrefs() { return meter_prefs; }
    const MeterPreferences& meterPrefs() const { return meter_prefs; }

private:
    // Apply/poll internals (faithful ports of the original GUI logic).
    bool WriteAllMasterVolumes();
    void PollHardware();
    void PollMasterVolumes();
    void PollInputMatrix();
    void PollPlaybackMatrix();
    void CheckServiceStatus();
    void ApplyOscCommand(const OscCommand& cmd);
    void SendOscState();

    std::unique_ptr<AlsaCore> alsa_;
    std::unique_ptr<OscServer> osc;
    OscPreferences osc_prefs;
    MeterPreferences meter_prefs;
    ServiceStatus service_status = ServiceStatus::NotRunning;

    // Submix selection: the output (0-17) whose mix the input/playback rows currently edit.
    int selected_output = 0;

    // Mixer domain state.
    std::vector<ChannelState> master_states;                 // 18 masters
    std::map<std::pair<int, int>, long> input_matrix_cache;  // (out, src) -> gain
    std::map<std::pair<int, int>, long> playback_matrix_cache;
    std::map<std::pair<int, int>, long> input_mute_state;    // present == muted; value = saved gain
    std::map<std::pair<int, int>, long> playback_mute_state;

    std::vector<std::chrono::steady_clock::time_point> master_last_write_time;
    std::chrono::steady_clock::time_point last_write_time;
    std::chrono::steady_clock::time_point last_poll_time;

    // OSC diff-push snapshots (sentinel -1 forces a first send; resync overrides).
    std::chrono::steady_clock::time_point last_osc_push_time;
    bool osc_resync = true;
    int osc_last_sent_submix = -1;
    std::vector<long> osc_last_out_fader, osc_last_in_fader, osc_last_pb_fader;
    std::vector<int> osc_last_out_mute, osc_last_out_solo, osc_last_out_link,
                     osc_last_in_mute, osc_last_pb_mute;

    // Held crosspoint hint (GUI drag protection).
    std::pair<int, int> held_cell{0, 0};
    bool has_held_cell = false;
};

} // namespace TotalMixer
