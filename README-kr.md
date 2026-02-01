# TotalMixer for Linux

RME Fireface 오디오 인터페이스를 위한 Linux용 GUI 믹서 애플리케이션.

## 지원 하드웨어

- RME Fireface 400

## 종속성

### 시스템 라이브러리

- ALSA (`libasound`)
- X11 및 확장 (`libx11`, `libxrandr`, `libxinerama`, `libxcursor`, `libxi`)
- OpenGL
- libsystemd

### 런타임 종속성

**필수:** [snd-firewire-ctl-services](https://github.com/alsa-project/snd-firewire-ctl-services)

이 프로젝트는 Fireface 하드웨어의 ALSA 컨트롤을 관리하기 위해 `snd-fireface-ctl-service`가 필요합니다. GUI를 실행하기 전에 서비스가 실행 중이어야 합니다.

## 설치

### 1. 시스템 종속성 설치

#### Arch Linux
```bash
sudo pacman -S alsa-lib libx11 libxrandr libxinerama libxcursor libxi systemd
```

#### Ubuntu/Debian
```bash
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libsystemd-dev
```

### 2. snd-firewire-ctl-services 설치

다음 링크의 설치 가이드를 따르십시오:
https://github.com/alsa-project/snd-firewire-ctl-services

설치 후 서비스를 활성화하고 시작하십시오:

```bash
systemctl --user enable snd-fireface-ctl.service
systemctl --user start snd-fireface-ctl.service
```

서비스 실행 확인:
```bash
systemctl --user status snd-fireface-ctl.service
```

### 3. TotalMixer 빌드

```bash
mkdir build
cd build
cmake ..
make
```

## 사용법

```bash
./build/totalmixer_gui
```

`snd-fireface-ctl.service`가 실행 중이지 않으면 GUI에 오류가 표시됩니다.

## 라이선스

이 프로젝트는 GNU General Public License v3.0에 따라 라이선스가 부여됩니다 - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하십시오.
