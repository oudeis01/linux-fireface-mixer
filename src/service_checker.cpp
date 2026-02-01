#include "service_checker.hpp"
#include <cstdlib>
#include <sstream>
#include <systemd/sd-bus.h>
#include <cstring>

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
    std::string cmd = "pgrep -x " + process_name + " > /dev/null 2>&1";
    
    int ret = system(cmd.c_str());
    return (ret == 0) ? ServiceStatus::Running : ServiceStatus::NotRunning;
}

ServiceStatus ServiceChecker::check_systemd(const std::string& service_name) {
    sd_bus *bus = nullptr;
    int r = sd_bus_default_user(&bus);
    if (r < 0) {
        return ServiceStatus::NotRunning;
    }

    sd_bus_message *reply = nullptr;
    r = sd_bus_call_method(
        bus,
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        "GetUnit",
        nullptr,
        &reply,
        "s",
        service_name.c_str()
    );

    if (r < 0) {
        sd_bus_unref(bus);
        return ServiceStatus::NotInstalled;
    }

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

bool ServiceChecker::try_start(const std::string& service_name) {
    sd_bus *bus = nullptr;
    int r = sd_bus_default_user(&bus);
    if (r < 0) {
        return false;
    }

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
    return (r >= 0);
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
