#pragma once

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>

namespace TotalMixer {

// One decoded inbound OSC control change, applied on the GUI thread.
enum class OscCmdType {
    OutFader, OutMute, OutSolo, OutLink,
    InFader,  InMute,
    PbFader,  PbMute,
    SubmixSelect,
    QueryAll,
    Unknown
};

struct OscCommand {
    OscCmdType type = OscCmdType::Unknown;
    int index = 0;       // 0-based channel (already converted from the 1-based OSC path)
    float value = 0.0f;  // normalized 0..1 for faders; 0/1 for toggles; ignored otherwise
};

// UDP OSC endpoint. A liblo server thread parses inbound messages into OscCommands that the
// GUI thread drains and applies; the GUI thread sends feedback back to the discovered client.
// All mixer state lives on the GUI thread, so only the command queue and the client address are
// shared across threads. The liblo handles are kept as void* so <lo/lo.h> stays out of this header.
class OscServer {
public:
    OscServer();
    ~OscServer();

    OscServer(const OscServer&) = delete;
    OscServer& operator=(const OscServer&) = delete;

    // Bind the incoming UDP port and start the receive thread. out_port is where feedback is sent
    // (to whatever host first contacts us). Returns false if the port could not be bound.
    bool Start(int in_port, int out_port);
    void Stop();
    bool IsRunning() const { return running_.load(); }

    // Move all queued inbound commands out (GUI thread).
    std::vector<OscCommand> DrainCommands();

    // True if at least one client has been discovered.
    bool HasClient() const;
    // Returns true once after the client address changes (e.g. a new controller connected),
    // so the caller can trigger a full state resync. Resets the flag.
    bool TakeClientChanged();

    // Send a single float feedback message to the current client (GUI thread). No-op if none.
    void SendFloat(const std::string& path, float value);

    // Called by the (file-local) liblo handler; pushes a parsed command and registers the client.
    void EnqueueCommand(const OscCommand& cmd, const char* client_host);

private:
    void* server_ = nullptr;  // lo_server_thread
    void* client_ = nullptr;  // lo_address
    int out_port_ = 9001;

    mutable std::mutex queue_mtx_;
    std::queue<OscCommand> queue_;

    mutable std::mutex client_mtx_;
    std::string client_host_;
    std::atomic<bool> client_changed_{false};

    std::atomic<bool> running_{false};
};

} // namespace TotalMixer
