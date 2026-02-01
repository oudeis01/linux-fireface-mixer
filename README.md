# TotalMixer for Linux

GUI mixer application for RME Fireface audio interfaces on Linux.

## Supported Hardware

- RME Fireface 400

## Dependencies

### System Libraries

- ALSA (`libasound`)
- X11 and extensions (`libx11`, `libxrandr`, `libxinerama`, `libxcursor`, `libxi`)
- OpenGL
- libsystemd

### Runtime Dependency

**Required:** [snd-firewire-ctl-services](https://github.com/alsa-project/snd-firewire-ctl-services)

This project requires `snd-fireface-ctl-service` to manage ALSA controls for the Fireface hardware. The service must be running before launching the GUI.

## Installation

### 1. Install System Dependencies

#### Arch Linux
```bash
sudo pacman -S alsa-lib libx11 libxrandr libxinerama libxcursor libxi systemd
```

#### Ubuntu/Debian
```bash
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libsystemd-dev
```

### 2. Install snd-firewire-ctl-services

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

### 3. Build TotalMixer

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

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.
