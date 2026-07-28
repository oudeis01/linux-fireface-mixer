// `totalmixer info` subcommand: dump the card's ALSA controls for diagnostics.
//
// Uses AlsaCore directly (no engine, no OSC). Prints the card name and every control with its
// type/range. Handy for confirming the kernel service exposes the expected mixer controls.

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "alsa_core.hpp"
#include "cli_subcommands.hpp"

namespace {

void PrintUsage() {
    std::cout <<
        "Usage: totalmixer info [--card <index>]\n"
        "\n"
        "Dump the card's ALSA controls (name, type, range) for diagnostics.\n"
        "\n"
        "Options:\n"
        "  --card <index>   ALSA card index to inspect (default: auto-select first Fireface)\n"
        "  -h, --help       Show this help and exit\n";
}

} // namespace

namespace TotalMixer {

// argv[0] is "info"; options follow from index 1.
int RunInfo(int argc, char** argv) {
    int card_index = -1;  // -1 = auto-select first Fireface

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
            PrintUsage();
            return 0;
        } else if (std::strcmp(arg, "--card") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: --card requires a value\n";
                return 2;
            }
            const char* val = argv[++i];
            char* end = nullptr;
            long parsed = std::strtol(val, &end, 10);
            if (end == val || *end != '\0') {
                std::cerr << "Error: --card expects an integer, got '" << val << "'\n";
                return 2;
            }
            card_index = static_cast<int>(parsed);
        } else {
            std::cerr << "Error: unknown argument '" << arg << "'\n";
            PrintUsage();
            return 2;
        }
    }

    try {
        std::cout << "Initializing AlsaCore..." << std::endl;
        AlsaCore alsa(card_index);

        std::cout << "Card Name: " << alsa.get_card_name() << std::endl;

        auto controls = alsa.list_all_controls();
        std::cout << "Found " << controls.size() << " controls." << std::endl;

        for (const auto& [name, idx] : controls) {
            std::cout << "Control: " << name << " (Idx: " << idx << ")" << std::endl;
            auto info = alsa.get_control_info(name, idx);
            if (info) {
                std::cout << "  Type: " << info->type << ", Min: " << info->min
                          << ", Max: " << info->max << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

} // namespace TotalMixer
