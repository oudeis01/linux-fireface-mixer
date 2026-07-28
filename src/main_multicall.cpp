// `totalmixer` multicall entry point: a single GL-free headless binary that dispatches to a
// subcommand. Keeping the GUI (`totalmixer_gui`) as a separate target means this binary never
// links ImGui/GLFW/OpenGL/X11, so it still runs on a headless server.
//
//   totalmixer daemon [--osc-in P] [--osc-out P] [--card N]   headless OSC daemon
//   totalmixer info   [--card N]                              dump ALSA controls
//   totalmixer --help                                         this message
//
// Each subcommand receives the argv slice starting at its own name (argv + 1), so option
// parsing inside a subcommand is identical to a standalone program.

#include <cstring>
#include <iostream>

#include "cli_subcommands.hpp"

namespace {

void PrintUsage() {
    std::cout <<
        "Usage: totalmixer <command> [options]\n"
        "\n"
        "Headless control surface for the RME Fireface mixer. For the graphical app, run\n"
        "totalmixer_gui instead.\n"
        "\n"
        "Commands:\n"
        "  daemon    Run the headless OSC control daemon\n"
        "  info      Dump the card's ALSA controls for diagnostics\n"
        "\n"
        "Run 'totalmixer <command> --help' for command-specific options.\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 2;
    }

    const char* command = argv[1];

    if (std::strcmp(command, "-h") == 0 || std::strcmp(command, "--help") == 0) {
        PrintUsage();
        return 0;
    }

    // Hand the subcommand its own argv slice: argv[1] ("daemon"/"info") becomes its argv[0].
    if (std::strcmp(command, "daemon") == 0) {
        return TotalMixer::RunDaemon(argc - 1, argv + 1);
    }
    if (std::strcmp(command, "info") == 0) {
        return TotalMixer::RunInfo(argc - 1, argv + 1);
    }

    std::cerr << "Error: unknown command '" << command << "'\n\n";
    PrintUsage();
    return 2;
}
