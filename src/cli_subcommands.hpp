// Entry points for the `totalmixer` multicall binary's subcommands. Each is implemented in its
// own translation unit and receives the argv slice starting at the subcommand name (so argv[0]
// is e.g. "daemon" and options follow from index 1).
#pragma once

namespace TotalMixer {

// `totalmixer daemon [...]` - headless OSC control daemon (no GUI/display dependency).
int RunDaemon(int argc, char** argv);

// `totalmixer info [--card N]` - dump the card's ALSA controls for diagnostics.
int RunInfo(int argc, char** argv);

} // namespace TotalMixer
