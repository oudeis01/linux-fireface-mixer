#include "bridge_process.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

namespace TotalMixer {

namespace {

// Resolve a bare command name against PATH, returning true if an executable is found. Mirrors
// what execvp would search, so the availability check and the actual launch agree.
bool FoundOnPath(const char* name) {
    const char* path = std::getenv("PATH");
    if (!path || *path == '\0') return false;

    std::string dirs(path);
    size_t start = 0;
    while (start <= dirs.size()) {
        size_t colon = dirs.find(':', start);
        std::string dir = dirs.substr(start, colon == std::string::npos ? std::string::npos
                                                                         : colon - start);
        if (!dir.empty()) {
            std::string candidate = dir + "/" + name;
            if (access(candidate.c_str(), X_OK) == 0) {
                struct stat st;
                if (stat(candidate.c_str(), &st) == 0 && S_ISREG(st.st_mode)) return true;
            }
        }
        if (colon == std::string::npos) break;
        start = colon + 1;
    }
    return false;
}

} // namespace

BridgeProcess::~BridgeProcess() {
    Stop();
}

bool BridgeProcess::IsAvailable() {
    return FoundOnPath(kBinaryName);
}

bool BridgeProcess::Start(int osc_in_port, int osc_out_port, bool allow_external) {
    if (running()) return true;

    // Build the argv strings up front (before fork) so the child does no allocation between
    // fork and exec beyond what execvp needs.
    std::string send_port = std::to_string(osc_in_port);
    std::string recv_port = std::to_string(osc_out_port);

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        // Child: build argv and exec. --osc-send-port is the desktop's OSC receive port (where
        // the bridge sends commands); --osc-recv-port is where the desktop sends feedback.
        const char* argv[8];
        int n = 0;
        argv[n++] = kBinaryName;
        argv[n++] = "--osc-send-port";
        argv[n++] = send_port.c_str();
        argv[n++] = "--osc-recv-port";
        argv[n++] = recv_port.c_str();
        if (allow_external) argv[n++] = "--allow-external";
        argv[n++] = nullptr;

        execvp(kBinaryName, const_cast<char* const*>(argv));
        // Only reached if exec failed (e.g. binary vanished after the PATH check). 127 is the
        // conventional "command not found" code; the parent surfaces it via last_exit_code().
        _exit(127);
    }

    pid_ = pid;
    last_exit_code_ = -1;
    return true;
}

void BridgeProcess::Stop() {
    if (!running()) return;
    kill(pid_, SIGTERM);
    int status = 0;
    waitpid(pid_, &status, 0);
    pid_ = -1;
}

bool BridgeProcess::Poll() {
    if (!running()) return false;
    int status = 0;
    pid_t r = waitpid(pid_, &status, WNOHANG);
    if (r == pid_) {
        // Child terminated on its own. Record a clean exit code; leave -1 if it was signalled.
        last_exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        pid_ = -1;
    }
    return running();
}

} // namespace TotalMixer
