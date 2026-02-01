#pragma once
#include <string>

namespace TotalMixer {

enum class ServiceStatus {
    Running,      // service is active and running
    NotRunning,   // service is not running (can be started)
    Failed,       // service is in failed state
    NotInstalled  // systemd unit file not found
};

class ServiceChecker {
public:
    // quick check using process name (fast, no dependencies)
    static ServiceStatus check_quick(const std::string& service_name);
    
    // precise check using systemd DBus (requires libsystemd)
    static ServiceStatus check_systemd(const std::string& service_name);
    
    // attempt to start service via systemd --user
    static bool try_start(const std::string& service_name);
    
    // get user-friendly status message
    static std::string get_status_message(ServiceStatus status);
    
    // get help message with resolution steps
    static std::string get_help_message(ServiceStatus status);

private:
    // extract process name from service unit name
    static std::string extract_process_name(const std::string& service_name);
};

} // namespace TotalMixer
