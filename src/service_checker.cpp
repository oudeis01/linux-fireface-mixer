#include "service_checker.hpp"
#include <cstdlib>
#include <sstream>
#include <systemd/sd-bus.h>
#include <cstring>
#include <iostream>

namespace TotalMixer {

std::string ServiceChecker::extract_process_name(const std::string& service_name) {
    size_t dot_pos = service_name.find(".service");
    if (dot_pos != std::string::npos) {
        return service_name.substr(0, dot_pos);
    }
    return service_name;
}

ServiceStatus ServiceChecker::check_quick(const std::string& service_name) {
    std::string process_name = extract_process_name(service_name);
    // Use pgrep -f to avoid 15-character process name limit
    std::string cmd = "pgrep -f " + process_name + " > /dev/null 2>&1";
    
    int ret = system(cmd.c_str());
    return (ret == 0) ? ServiceStatus::Running : ServiceStatus::NotRunning;
}

ServiceStatus ServiceChecker::check_systemd(const std::string& service_name) {
    // Try D-Bus first
    sd_bus *bus = nullptr;
    int r = sd_bus_default_user(&bus);
    if (r >= 0) {
        sd_bus_message *reply = nullptr;
        sd_bus_error error = SD_BUS_ERROR_NULL;
        r = sd_bus_call_method(
            bus,
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            "GetUnit",
            &error,
            &reply,
            "s",
            service_name.c_str()
        );

        if (r >= 0) {
            const char *unit_path = nullptr;
            sd_bus_message_read(reply, "o", &unit_path);
            sd_bus_message_unref(reply);

            char *state = nullptr;
            r = sd_bus_get_property_string(
                bus,
                "org.freedesktop.systemd1",
                unit_path,
                "org.freedesktop.systemd1.Unit",
                "ActiveState",
                nullptr,
                &state
            );

            ServiceStatus status = ServiceStatus::NotRunning;
            if (r >= 0 && state != nullptr) {
                if (strcmp(state, "active") == 0) {
                    status = ServiceStatus::Running;
                } else if (strcmp(state, "failed") == 0) {
                    status = ServiceStatus::Failed;
                } else {
                    status = ServiceStatus::NotRunning;
                }
                free((void*)state);
            }

            sd_bus_unref(bus);
            return status;
        }
        sd_bus_error_free(&error);
        sd_bus_unref(bus);
    }

    // Fallback: use systemctl command
    std::string cmd = "systemctl --user is-active " + service_name + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return ServiceStatus::NotRunning;
    }

    char buffer[128];
    std::string result;
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = buffer;
    }
    pclose(pipe);

    // Remove trailing newline
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    if (result == "active") {
        return ServiceStatus::Running;
    } else if (result == "failed") {
        return ServiceStatus::Failed;
    } else if (result == "inactive" || result == "activating") {
        return ServiceStatus::NotRunning;
    } else {
        // Service not found or other error
        return ServiceStatus::NotInstalled;
    }
}

bool ServiceChecker::try_start(const std::string& service_name) {
    // Try D-Bus first
    sd_bus *bus = nullptr;
    int r = sd_bus_default_user(&bus);
    if (r >= 0) {
        r = sd_bus_call_method(
            bus,
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            "StartUnit",
            nullptr,
            nullptr,
            "ss",
            service_name.c_str(),
            "replace"
        );
        sd_bus_unref(bus);
        if (r >= 0) {
            return true;
        }
    }

    // Fallback: use systemctl command
    std::string cmd = "systemctl --user start " + service_name + " 2>/dev/null";
    int ret = system(cmd.c_str());
    return (ret == 0);
}

std::string ServiceChecker::get_status_message(ServiceStatus status) {
    switch (status) {
        case ServiceStatus::Running:
            return "Service is running";
        case ServiceStatus::NotRunning:
            return "Service is not running";
        case ServiceStatus::Failed:
            return "Service is in failed state";
        case ServiceStatus::NotInstalled:
            return "Service is not installed";
        default:
            return "Unknown service status";
    }
}

std::string ServiceChecker::get_help_message(ServiceStatus status) {
    switch (status) {
        case ServiceStatus::NotRunning:
            return "Try: systemctl --user start snd-fireface-ctl.service";
        case ServiceStatus::Failed:
            return "Check logs: journalctl --user -u snd-fireface-ctl.service -n 50";
        case ServiceStatus::NotInstalled:
            return "Install snd-fireface-ctl-service and create systemd user unit";
        case ServiceStatus::Running:
            return "Service is healthy";
        default:
            return "";
    }
}

} // namespace TotalMixer
