# TotalMixer for Linux

GUI mixer application for RME Fireface audio interfaces on Linux.

## Supported Hardware

- RME Fireface 400

## Features

- **Mixer view** - canonical TotalMix-style 3-row layout (hardware inputs, software playback, hardware outputs). Selecting an output edits its submix.
- **Matrix view** - full crosspoint grid, kept in sync with the mixer view.
- **Level meters** - hardware metering with a -90 dBFS range, RMS bar plus peak-hold line, and overload indication.
- **Per-channel Mute / Solo / stereo Link** on outputs, and per-submix Mute on input/playback sources.
- **OSC remote control** - bidirectional Open Sound Control endpoint to drive and observe the mixer over the network (see Usage).

## Dependencies

### System Libraries

- ALSA (`libasound`)
- X11 and extensions (`libx11`, `libxrandr`, `libxinerama`, `libxcursor`, `libxi`)
- OpenGL
- libsystemd
- liblo (OSC)

### Runtime Dependency

**Required:** [snd-firewire-ctl-services](https://github.com/alsa-project/snd-firewire-ctl-services)

This project requires `snd-fireface-ctl-service` to manage ALSA controls for the Fireface hardware. The service must be running before launching the GUI.

## Installation

### Arch Linux (AUR)

#### Binary Package (Recommended)
```bash
yay -S linux-fireface-mixer-bin
```

#### Source Package
```bash
yay -S linux-fireface-mixer
```

### Manual Build

#### 1. Install System Dependencies

#### Arch Linux
```bash
sudo pacman -S alsa-lib libx11 libxrandr libxinerama libxcursor libxi systemd liblo
```

#### Ubuntu/Debian
```bash
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libsystemd-dev liblo-dev
```

#### 2. Install snd-firewire-ctl-services

Follow the installation instructions at:
https://github.com/alsa-project/snd-firewire-ctl-services

After installation, enable and start the service:

```bash
systemctl --user enable snd-fireface-ctl.service
systemctl --user start snd-fireface-ctl.service
```

Verify the service is running:
```bash
systemctl --user status snd-fireface-ctl.service
```

#### 3. Build TotalMixer

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

```bash
./build/totalmixer_gui
```

The GUI will display an error if `snd-fireface-ctl.service` is not running.

### OSC Remote Control

Enable the OSC server under **Control tab -> [ OSC Remote ]** (default: receive on UDP 7001, send feedback on 9001). Values are normalized floats in `0.0 .. 1.0`; toggles use `0`/`1`.

Address namespace (receive and send are symmetric):

| Address | Meaning |
| --- | --- |
| `/out/fader/N` `/out/mute/N` `/out/solo/N` `/out/link/N` | Output N fader / mute / solo / stereo link |
| `/in/fader/N` `/in/mute/N` | Input N source gain / mute in the current submix |
| `/pb/fader/N` `/pb/mute/N` | Playback N source gain / mute in the current submix |
| `/submix/select/N` (recv) | Switch the submix being edited |
| `/submix/current` (send) | Currently active submix number |
| `/query` (recv) | Request a full state dump |

The desktop sends feedback to the first host that contacts it, so a controller should send any message (e.g. `/query`) once to register. Quick test with the `liblo` CLI tools:

```bash
oscsend 127.0.0.1 7001 /out/fader/1 f 0.5   # set output 1 fader to mid
oscdump 9001                                 # observe feedback from the desktop
```

> Note: the endpoint binds all interfaces and is unauthenticated UDP. Use it only on a trusted LAN.

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.
