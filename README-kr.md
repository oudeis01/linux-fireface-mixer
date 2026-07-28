# TotalMixer for Linux

RME Fireface 오디오 인터페이스를 위한 Linux용 GUI 믹서 애플리케이션.

## 지원 하드웨어

- RME Fireface 400

## 기능

- **믹서 뷰** - 정본 TotalMix 방식의 3행 레이아웃(하드웨어 입력, 소프트웨어 재생, 하드웨어 출력). 출력을 선택하면 해당 서브믹스를 편집합니다.
- **매트릭스 뷰** - 전체 크로스포인트 그리드. 믹서 뷰와 동기화됩니다.
- **레벨 미터** - -90 dBFS 범위의 하드웨어 미터링, RMS 바 + 피크 홀드 라인, 오버로드 표시.
- **채널별 Mute / Solo / 스테레오 Link** (출력), 입력/재생 소스에 대한 서브믹스별 Mute.
- **OSC 원격 제어** - 네트워크로 믹서를 제어하고 상태를 관찰하는 양방향 Open Sound Control 엔드포인트 (사용법 참조).
- **헤드리스 데몬** - `totalmixer daemon`은 GUI나 디스플레이 의존 없이 동일한 OSC 엔드포인트를 제공하여, 헤드리스 서버에서 믹서를 상주 실행합니다 (사용법 참조).
- **웹 리모트** - 별도 `linux-totalmix-web-remote` 패키지가 설치되어 있으면 GUI에서 실행할 수 있는, 폰/태블릿 브라우저 기반 선택적 제어 (사용법 참조).

## 종속성

### 시스템 라이브러리

- ALSA (`libasound`)
- X11 및 확장 (`libx11`, `libxrandr`, `libxinerama`, `libxcursor`, `libxi`)
- OpenGL
- libsystemd
- liblo (OSC)

### 런타임 종속성

**필수:** [snd-firewire-ctl-services](https://github.com/alsa-project/snd-firewire-ctl-services)

이 프로젝트는 Fireface 하드웨어의 ALSA 컨트롤을 관리하기 위해 `snd-fireface-ctl-service`가 필요합니다. GUI를 실행하기 전에 서비스가 실행 중이어야 합니다.

## 설치

### Arch Linux (AUR)

#### 바이너리 패키지 (권장)
```bash
yay -S linux-fireface-mixer-bin
```

#### 소스 패키지
```bash
yay -S linux-fireface-mixer
```

### 수동 빌드

#### 1. 시스템 종속성 설치

#### Arch Linux
```bash
sudo pacman -S alsa-lib libx11 libxrandr libxinerama libxcursor libxi systemd liblo
```

#### Ubuntu/Debian
```bash
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libsystemd-dev liblo-dev
```

#### 2. snd-firewire-ctl-services 설치

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

#### 3. TotalMixer 빌드

```bash
mkdir build
cd build
cmake ..
make
```

## 사용법

```bash
./build/totalmixer_gui          # GUI 믹서
./build/totalmixer info         # 카드 컨트롤을 stdout에 덤프 (--card N으로 카드 선택)
./build/totalmixer daemon       # 헤드리스 OSC 데몬 (아래 참조)
```

`snd-fireface-ctl.service`가 실행 중이지 않으면 GUI에 오류가 표시됩니다.

헤드리스 `totalmixer` 바이너리는 멀티콜 실행물입니다: `totalmixer <command>`. 명령 목록은 `totalmixer --help`로 확인하십시오.

> **0.3.x에서 업그레이드:** 별도였던 `totalmixer-daemon`과 `totalmixer_cli` 바이너리가 사라졌습니다. `totalmixer-daemon`은 이제 `totalmixer daemon`, 기존 CLI의 컨트롤 덤프는 `totalmixer info`입니다. 배포되는 systemd 유닛 파일명은 그대로(`totalmixer-daemon.service`)이며 `ExecStart`만 `totalmixer daemon`으로 바뀌었습니다. 직접 만든 스크립트나 drop-in이 있다면 맞춰 수정하십시오.

### OSC 원격 제어

**Control 탭 -> [ OSC Remote ]** 에서 OSC 서버를 활성화합니다 (기본값: UDP 7001 수신, 9001 피드백 송신). 값은 `0.0 .. 1.0` 정규화 float이며, 토글은 `0`/`1`을 사용합니다.

주소 체계 (수신과 송신이 대칭):

| 주소 | 의미 |
| --- | --- |
| `/out/fader/N` `/out/mute/N` `/out/solo/N` `/out/link/N` | 출력 N 페이더 / 뮤트 / 솔로 / 스테레오 링크 |
| `/in/fader/N` `/in/mute/N` | 현재 서브믹스의 입력 N 소스 게인 / 뮤트 |
| `/pb/fader/N` `/pb/mute/N` | 현재 서브믹스의 재생 N 소스 게인 / 뮤트 |
| `/submix/select/N` (수신) | 편집 대상 서브믹스 전환 |
| `/submix/current` (송신) | 현재 활성 서브믹스 번호 |
| `/query` (수신) | 전체 상태 덤프 요청 |

데스크탑은 처음 접속한 호스트로 피드백을 보냅니다. 따라서 컨트롤러는 등록을 위해 메시지를 한 번(예: `/query`) 보내야 합니다. `liblo` CLI 도구로 간단히 테스트:

```bash
oscsend 127.0.0.1 7001 /out/fader/1 f 0.5   # 출력 1 페이더를 중간으로
oscdump 9001                                 # 데스크탑이 보내는 피드백 관찰
```

> 참고: 엔드포인트는 모든 인터페이스에 바인딩되며 인증 없는 UDP입니다. 신뢰된 LAN에서만 사용하십시오.

### 헤드리스 데몬

헤드리스 서버(X11/OpenGL 없음)를 위해 `totalmixer daemon`은 GUI 없이 동일한 OSC 엔드포인트를 실행합니다. 미터와 디스플레이 의존이 없습니다. 데몬 모드에서 OSC는 항상 활성화됩니다(`preferences.json`의 `enabled` 플래그는 무시).

```bash
./build/totalmixer daemon                       # preferences.json의 포트 사용
./build/totalmixer daemon --osc-in 7005 --osc-out 9005 --card 1
./build/totalmixer daemon --help
```

`snd-fireface-ctl.service`가 실행 중이 아니거나 카드를 사용할 수 없으면 데몬은 0이 아닌 코드로 종료하므로, 재시도 정책은 서비스 관리자가 담당합니다. systemd **user** 유닛이 설치됩니다(기본 비활성):

```bash
systemctl --user enable --now totalmixer-daemon.service
systemctl --user status totalmixer-daemon.service
```

포트나 카드를 바꾸려면 배포된 유닛을 직접 수정하지 말고 drop-in을 사용하십시오:

```bash
systemctl --user edit totalmixer-daemon.service
# [Service]
# ExecStart=
# ExecStart=/usr/bin/totalmixer daemon --osc-in 7005 --osc-out 9005 --card 1
```

> GUI 인스턴스가 OSC 서버를 켠 상태에서 데몬을 동시에 실행하지 마십시오. 둘이 같은 UDP 포트를 바인딩하려다 충돌합니다. 하나만 사용하거나 서로 다른 포트를 지정하십시오.

### 웹 리모트

**웹 리모트**는 LAN을 통해 폰이나 태블릿 브라우저에서 믹서를 제어합니다. 별도 프로그램인 `linux-totalmix-web-remote`로 독립 패키징되어 있습니다(`yay -S linux-totalmix-web-remote-bin`). `linux-fireface-mixer` 패키지는 이를 선택적 의존(optdepends)으로 명시합니다.

설치되어 `PATH`에 있으면, GUI의 **Control 탭 -> [ Web Remote ]** 에서 **Start**를 누르십시오. GUI가 브릿지를 자식 프로세스로 실행하며 믹서의 현재 OSC 포트에 연결하고, 접속할 LAN URL을 표시합니다(기본 `http://<내-IP>:8451`). 중지하거나 GUI를 닫으면 브릿지도 종료됩니다.

헤드리스 데몬 환경에서는 GUI가 개입하지 않으므로, 브릿지를 자체 systemd user 유닛으로 실행하십시오:

```bash
linux-totalmix-web-remote systemd install --enable --now
```

> 웹 UI는 인증이 없으며 OSC 엔드포인트와 동일한 신뢰 모델을 따릅니다. 신뢰된 LAN에서만 사용하십시오.

## 라이선스

이 프로젝트는 GNU General Public License v3.0에 따라 라이선스가 부여됩니다 - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하십시오.
