#include "mixer_engine.hpp"
#include "config_manager.hpp"
#include <iostream>

namespace TotalMixer {

using std::chrono::steady_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;

static inline long clamp_gain(long v) { return v < 0 ? 0 : (v > 65536 ? 65536 : v); }

MixerEngine::MixerEngine()
    : last_write_time(steady_clock::now()),
      last_poll_time(steady_clock::now()),
      last_osc_push_time(steady_clock::now()) {
    master_states.resize(18);
    master_last_write_time.resize(18, steady_clock::now() - std::chrono::seconds(10));

    // OSC feedback diff snapshots (sentinel -1 forces a first send; resync also overrides).
    osc_last_out_fader.assign(18, -1);
    osc_last_in_fader.assign(18, -1);
    osc_last_pb_fader.assign(18, -1);
    osc_last_out_mute.assign(18, -1);
    osc_last_out_solo.assign(18, -1);
    osc_last_out_link.assign(18, -1);
    osc_last_in_mute.assign(18, -1);
    osc_last_pb_mute.assign(18, -1);

    // Load persisted preferences (both meter and OSC blocks share preferences.json).
    ConfigManager::Load(meter_prefs, osc_prefs);
}

MixerEngine::~MixerEngine() {}

// ── Startup ──
void MixerEngine::CheckServiceStatus() {
    service_status = ServiceChecker::check_systemd("snd-fireface-ctl.service");
    if (service_status == ServiceStatus::NotInstalled) {
        std::cerr << "Engine Warning: snd-fireface-ctl.service not found" << std::endl;
    }
}

MixerEngine::InitResult MixerEngine::Init(int card_index) {
    CheckServiceStatus();
    if (service_status != ServiceStatus::Running) {
        std::cerr << "Engine Error: snd-fireface-ctl.service is not running" << std::endl;
        return InitResult{false, service_status};
    }
    try {
        alsa_ = std::make_unique<AlsaCore>(card_index);
        std::cout << "Engine: Connected to " << alsa_->get_card_name() << std::endl;
        PollHardware();
        return InitResult{true, service_status};
    } catch (const std::exception& e) {
        std::cerr << "Engine Warning: Failed to connect to ALSA: " << e.what() << std::endl;
        alsa_.reset();
        return InitResult{false, service_status};
    }
}

// ── Submix helpers ──
int MixerEngine::OutputLinkPartner(int ch) const {
    if (ch < 0 || ch >= (int)master_states.size()) return -1;
    if (!master_states[ch].is_linked) return -1;
    int partner = (ch % 2 == 0) ? ch + 1 : ch - 1;
    if (partner < 0 || partner >= (int)master_states.size()) return -1;
    return partner;
}

bool MixerEngine::IsOutputSelected(int ch) const {
    if (ch == selected_output) return true;
    int partner = OutputLinkPartner(selected_output);
    return (partner != -1 && ch == partner);
}

// ── Crosspoint write ──
bool MixerEngine::WriteSourceGain(bool is_playback, int src_idx, int output, long val) {
    if (!alsa_) return false;
    std::string mixer_name = is_playback ? "mixer:stream-source-gain" :
                             (src_idx < 8 ? "mixer:analog-source-gain" :
                             (src_idx < 10 ? "mixer:spdif-source-gain" : "mixer:adat-source-gain"));
    int hw_in_idx = is_playback ? src_idx : (src_idx < 8 ? src_idx : (src_idx < 10 ? src_idx - 8 : src_idx - 10));
    return alsa_->set_matrix_gain(mixer_name, hw_in_idx, output, val);
}

// ── Shared apply primitives ──
bool MixerEngine::WriteAllMasterVolumes() {
    if (!alsa_) return false;
    std::vector<long> all_v(18);
    bool any_solo = false;
    for (int i = 0; i < 18; ++i) {
        if (master_states[i].is_soloed) { any_solo = true; break; }
    }
    for (int i = 0; i < 18; ++i) {
        all_v[i] = (any_solo && !master_states[i].is_soloed) ? 0 : master_states[i].value;
    }
    return alsa_->set_control_value("output-volume", 0, all_v);
}

void MixerEngine::SetMasterVolume(int ch, long val) {
    if (ch < 0 || ch >= 18) return;
    val = clamp_gain(val);
    auto now = steady_clock::now();
    master_states[ch].value = val;
    master_states[ch].is_muted = false;  // an explicit level set clears mute
    master_last_write_time[ch] = now;
    int partner = OutputLinkPartner(ch);
    if (partner != -1) {
        master_states[partner].value = val;
        master_states[partner].is_muted = false;
        master_last_write_time[partner] = now;
    }
    if (WriteAllMasterVolumes()) last_write_time = now;
}

void MixerEngine::SetMasterMute(int ch, bool mute) {
    if (ch < 0 || ch >= 18) return;
    if (master_states[ch].is_muted == mute) return;
    int partner = OutputLinkPartner(ch);
    auto apply = [&](int c) {
        if (mute) {
            master_states[c].is_muted = true;
            master_states[c].saved_value = master_states[c].value;
            master_states[c].value = 0;
        } else {
            master_states[c].is_muted = false;
            master_states[c].value = clamp_gain(master_states[c].saved_value);
        }
    };
    apply(ch);
    if (partner != -1) apply(partner);
    auto now = steady_clock::now();
    master_last_write_time[ch] = now;
    if (partner != -1) master_last_write_time[partner] = now;
    if (WriteAllMasterVolumes()) last_write_time = now;
}

void MixerEngine::SetMasterSolo(int ch, bool solo) {
    if (ch < 0 || ch >= 18) return;
    master_states[ch].is_soloed = solo;
    int partner = OutputLinkPartner(ch);
    if (partner != -1) master_states[partner].is_soloed = solo;
    auto now = steady_clock::now();
    master_last_write_time[ch] = now;
    if (partner != -1) master_last_write_time[partner] = now;
    if (WriteAllMasterVolumes()) last_write_time = now;
}

void MixerEngine::SetMasterLink(int ch, bool linked) {
    if (ch < 0 || ch >= 18) return;
    master_states[ch].is_linked = linked;
    int pair = (ch % 2 == 0) ? ch + 1 : ch - 1;
    if (pair >= 0 && pair < 18) master_states[pair].is_linked = linked;
}

void MixerEngine::SetSourceGain(bool is_playback, int src_idx, int output, long val) {
    if (src_idx < 0 || src_idx >= 18 || output < 0 || output >= 18) return;
    val = clamp_gain(val);
    auto& cache = is_playback ? playback_matrix_cache : input_matrix_cache;
    auto& mute_state = is_playback ? playback_mute_state : input_mute_state;
    if (val > 0) mute_state.erase({output, src_idx});  // raising level clears mute
    cache[{output, src_idx}] = val;
    WriteSourceGain(is_playback, src_idx, output, val);
    int partner = OutputLinkPartner(output);
    if (partner != -1) {
        cache[{partner, src_idx}] = val;
        WriteSourceGain(is_playback, src_idx, partner, val);
    }
    last_write_time = steady_clock::now();
}

void MixerEngine::SetSourceMute(bool is_playback, int src_idx, int output, bool mute) {
    if (src_idx < 0 || src_idx >= 18 || output < 0 || output >= 18) return;
    auto& cache = is_playback ? playback_matrix_cache : input_matrix_cache;
    auto& mute_state = is_playback ? playback_mute_state : input_mute_state;
    bool cur = mute_state.count({output, src_idx}) > 0;
    if (cur == mute) return;
    int partner = OutputLinkPartner(output);
    if (mute) {
        mute_state[{output, src_idx}] = cache[{output, src_idx}];
        WriteSourceGain(is_playback, src_idx, output, 0);
        cache[{output, src_idx}] = 0;
        if (partner != -1) {
            WriteSourceGain(is_playback, src_idx, partner, 0);
            cache[{partner, src_idx}] = 0;
        }
    } else {
        long saved = mute_state[{output, src_idx}];
        mute_state.erase({output, src_idx});
        WriteSourceGain(is_playback, src_idx, output, saved);
        cache[{output, src_idx}] = saved;
        if (partner != -1) {
            WriteSourceGain(is_playback, src_idx, partner, saved);
            cache[{partner, src_idx}] = saved;
        }
    }
    last_write_time = steady_clock::now();
}

void MixerEngine::SetSubmix(int output) {
    if (output < 0 || output >= 18) return;
    selected_output = output;
    osc_resync = true;
}

long MixerEngine::sourceGain(bool is_playback, int output, int src_idx) const {
    const auto& cache = is_playback ? playback_matrix_cache : input_matrix_cache;
    auto it = cache.find({output, src_idx});
    return it == cache.end() ? 0 : it->second;
}

bool MixerEngine::sourceMuted(bool is_playback, int output, int src_idx) const {
    const auto& mute_state = is_playback ? playback_mute_state : input_mute_state;
    return mute_state.count({output, src_idx}) > 0;
}

long& MixerEngine::crosspoint(bool is_playback, int output, int src_idx) {
    auto& cache = is_playback ? playback_matrix_cache : input_matrix_cache;
    return cache[{output, src_idx}];
}

bool MixerEngine::WriteCrosspointRaw(bool is_playback, int src_idx, int output, long val) {
    bool ok = WriteSourceGain(is_playback, src_idx, output, val);
    if (ok) last_write_time = steady_clock::now();
    return ok;
}

void MixerEngine::SetHeldCrosspoint(int output, int src_idx) {
    held_cell = {output, src_idx};
    has_held_cell = true;
}

void MixerEngine::ClearHeldCrosspoint() {
    has_held_cell = false;
}

bool MixerEngine::isHeldCrosspoint(int output, int src_idx) const {
    return has_held_cell && held_cell.first == output && held_cell.second == src_idx;
}

bool MixerEngine::oscRunning() const {
    return osc && osc->IsRunning();
}

bool MixerEngine::oscHasClient() const {
    return osc && osc->IsRunning() && osc->HasClient();
}

// ── OSC endpoint glue ──
void MixerEngine::RestartOscServer() {
    if (!osc) osc = std::make_unique<OscServer>();
    osc->Stop();
    if (osc_prefs.enabled) {
        osc->Start(osc_prefs.in_port, osc_prefs.out_port);
    }
    osc_resync = true;  // force a full state dump once a client appears
}

void MixerEngine::StopOsc() {
    if (osc) osc->Stop();
}

void MixerEngine::ApplyOscCommand(const OscCommand& cmd) {
    const long raw = clamp_gain((long)(cmd.value * 65536.0f + 0.5f));
    const bool on = cmd.value > 0.5f;
    switch (cmd.type) {
        case OscCmdType::OutFader: SetMasterVolume(cmd.index, raw); break;
        case OscCmdType::OutMute:  SetMasterMute(cmd.index, on); break;
        case OscCmdType::OutSolo:  SetMasterSolo(cmd.index, on); break;
        case OscCmdType::OutLink:  SetMasterLink(cmd.index, on); break;
        case OscCmdType::InFader:  SetSourceGain(false, cmd.index, selected_output, raw); break;
        case OscCmdType::InMute:   SetSourceMute(false, cmd.index, selected_output, on); break;
        case OscCmdType::PbFader:  SetSourceGain(true, cmd.index, selected_output, raw); break;
        case OscCmdType::PbMute:   SetSourceMute(true, cmd.index, selected_output, on); break;
        case OscCmdType::SubmixSelect:
            if (cmd.index >= 0 && cmd.index < 18) { selected_output = cmd.index; osc_resync = true; }
            break;
        case OscCmdType::QueryAll: osc_resync = true; break;
        default: break;
    }
}

// Diff-based feedback: send only the control values that changed since the last push (or all of
// them on a forced resync). One path covers UI edits, hardware poll changes, and OSC-applied
// changes uniformly. Source rows are view-coupled to the currently selected submix.
void MixerEngine::SendOscState() {
    if (!osc || !osc->IsRunning() || !osc->HasClient()) return;

    bool full = osc_resync || (selected_output != osc_last_sent_submix);
    const float N = 65536.0f;
    auto sendf = [&](const std::string& p, float v) { osc->SendFloat(p, v); };

    if (selected_output != osc_last_sent_submix) {
        sendf("/submix/current", (float)(selected_output + 1));
        osc_last_sent_submix = selected_output;
    }

    for (int i = 0; i < 18; ++i) {
        std::string n = std::to_string(i + 1);

        long ov = master_states[i].value;
        if (full || ov != osc_last_out_fader[i]) { sendf("/out/fader/" + n, ov / N); osc_last_out_fader[i] = ov; }
        int om = master_states[i].is_muted ? 1 : 0;
        if (full || om != osc_last_out_mute[i]) { sendf("/out/mute/" + n, (float)om); osc_last_out_mute[i] = om; }
        int os = master_states[i].is_soloed ? 1 : 0;
        if (full || os != osc_last_out_solo[i]) { sendf("/out/solo/" + n, (float)os); osc_last_out_solo[i] = os; }
        int ol = master_states[i].is_linked ? 1 : 0;
        if (full || ol != osc_last_out_link[i]) { sendf("/out/link/" + n, (float)ol); osc_last_out_link[i] = ol; }

        long iv = input_matrix_cache[{selected_output, i}];
        if (full || iv != osc_last_in_fader[i]) { sendf("/in/fader/" + n, iv / N); osc_last_in_fader[i] = iv; }
        int im = input_mute_state.count({selected_output, i}) ? 1 : 0;
        if (full || im != osc_last_in_mute[i]) { sendf("/in/mute/" + n, (float)im); osc_last_in_mute[i] = im; }

        long pv = playback_matrix_cache[{selected_output, i}];
        if (full || pv != osc_last_pb_fader[i]) { sendf("/pb/fader/" + n, pv / N); osc_last_pb_fader[i] = pv; }
        int pm = playback_mute_state.count({selected_output, i}) ? 1 : 0;
        if (full || pm != osc_last_pb_mute[i]) { sendf("/pb/mute/" + n, (float)pm); osc_last_pb_mute[i] = pm; }
    }
    osc_resync = false;
}

// ── Hardware polling ──
void MixerEngine::PollHardware() {
    if (!alsa_) return;
    try {
        PollMasterVolumes();
        PollPlaybackMatrix();
        PollInputMatrix();
    } catch (...) {}
}

void MixerEngine::PollMasterVolumes() {
    if (!alsa_) return;
    try {
        // While any channel is soloed, the hardware output-volume of non-soloed channels is
        // driven to 0 (solo suppression), not their true fader value. Polling then would read
        // those 0s back into master_states and destroy the saved values, so solo-release can no
        // longer restore them. Skip the whole master poll while solo is active.
        for (int i = 0; i < 18; ++i) {
            if (master_states[i].is_soloed) return;
        }
        auto mv = alsa_->get_matrix_row("output-volume", 0, 18);
        if (mv) {
            auto now = steady_clock::now();
            for (size_t i = 0; i < mv->size() && i < 18; ++i) {
                // Skip if muted or soloed (user control in progress)
                if (master_states[i].is_muted || master_states[i].is_soloed) continue;
                // Skip updating if this specific fader was written to in the last 2000ms
                auto elapsed = duration_cast<milliseconds>(now - master_last_write_time[i]).count();
                if (elapsed < 2000) continue;

                master_states[i].value = (*mv)[i];
            }
        }
    } catch (...) {}
}

void MixerEngine::PollInputMatrix() {
    try {
        std::vector<std::string> ctl_names = {"mixer:analog-source-gain", "mixer:spdif-source-gain", "mixer:adat-source-gain"};
        std::vector<int> offsets = {0, 8, 10};
        for (size_t grp = 0; grp < ctl_names.size(); ++grp) {
            int base_in = offsets[grp];
            int count = (grp == 0) ? 8 : ((grp == 1) ? 2 : 8);
            for (int local_in = 0; local_in < count; ++local_in) {
                auto r = alsa_->get_matrix_row(ctl_names[grp], local_in, 18);
                if (r) {
                    int global_in = base_in + local_in;
                    for (size_t o = 0; o < r->size(); ++o) {
                        if (has_held_cell &&
                            held_cell.first == static_cast<int>(o) &&
                            held_cell.second == global_in) {
                            continue;
                        }
                        input_matrix_cache[{static_cast<int>(o), global_in}] = (*r)[o];
                    }
                }
            }
        }
    } catch (...) {}
}

void MixerEngine::PollPlaybackMatrix() {
    try {
        for (int o = 0; o < 18; ++o) {
            auto r_pb = alsa_->get_matrix_row("mixer:stream-source-gain", o, 18);
            if (r_pb) {
                for (size_t i = 0; i < r_pb->size(); ++i) {
                    if (has_held_cell &&
                        held_cell.first == static_cast<int>(i) &&
                        held_cell.second == o) {
                        continue;
                    }
                    playback_matrix_cache[{static_cast<int>(i), o}] = (*r_pb)[i];
                }
            }
        }
    } catch (...) {}
}

// ── Service cycle ──
void MixerEngine::Tick(bool inputs_busy) {
    auto now = steady_clock::now();

    // OSC inbound: apply any queued remote commands on this thread.
    if (osc && osc->IsRunning()) {
        if (osc->TakeClientChanged()) osc_resync = true;  // new controller -> full dump
        for (const auto& cmd : osc->DrainCommands()) ApplyOscCommand(cmd);
    }

    // Throttled hardware poll. Skip while inputs are busy (GUI drag) or right after a write.
    auto elapsed = duration_cast<milliseconds>(now - last_poll_time).count();
    auto since_write = duration_cast<milliseconds>(now - last_write_time).count();
    bool should_skip_poll = inputs_busy || (since_write < 200);
    if (elapsed > 500 && !should_skip_poll) {
        PollHardware();
        last_poll_time = now;
    }

    // OSC outbound: diff-push control state to the client at ~20Hz.
    auto osc_elapsed = duration_cast<milliseconds>(now - last_osc_push_time).count();
    if (osc_elapsed > 50) {
        SendOscState();
        last_osc_push_time = now;
    }
}

} // namespace TotalMixer
