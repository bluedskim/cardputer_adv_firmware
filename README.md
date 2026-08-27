# cardputer_adv_firmware

M5Stack Cardputer / Cardputer-Adv용 커스텀 펌웨어입니다. 기본 탑재 UI처럼 기능별로 좌우 전환하며, **Torch(손전등)** 와 **IR 리모컨** 기능을 제공합니다.

주 개발 환경은 **Fedora** 기준이며, 아래 설치·업로드 절차도 Fedora를 우선으로 적어 두었습니다.

## 개발 환경 (Fedora)

### 1. 시스템 패키지

```bash
sudo dnf install git python3-pip gcc-c++ make SDL2-devel pkg-config
```

| 패키지 | 용도 |
|--------|------|
| `git`, `python3-pip` | PlatformIO, 소스 클론 |
| `gcc-c++`, `make` | 네이티브/sim 빌드 |
| `SDL2-devel`, `pkg-config` | 데스크탑 시뮬레이터 (`sim`) |

### 2. PlatformIO

**Cursor / VS Code 확장** (권장): [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) 설치 후 저장소를 열면 `platformio.ini` 의존성을 자동으로 받습니다.

**CLI**만 쓸 때:

```bash
pip install --user platformio
# ~/.local/bin 이 PATH에 없으면 추가 (예: ~/.bashrc)
export PATH="$HOME/.local/bin:$PATH"
pio --version
```

저장소 루트에서 첫 빌드 시 ESP32 툴체인·라이브러리가 `~/.platformio` 아래로 내려받아집니다.

### 3. USB 업로드 · 시리얼 모니터

Cardputer-Adv(ESP32-S3)는 USB CDC로 `/dev/ttyACM*` 에 잡힙니다. 권한이 없으면 `pio run -t upload` 또는 `pio device monitor`가 실패합니다.

**방법 A — `dialout` 그룹 (일반적)**

```bash
sudo usermod -aG dialout "$USER"
# 로그아웃 후 다시 로그인 (또는 재부팅)
```

**방법 B — udev 규칙 (Espressif)**

`/etc/udev/rules.d/99-espressif.rules`:

```
SUBSYSTEM=="usb", ATTR{idVendor}=="303a", MODE="0666", GROUP="dialout"
```

적용:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
# USB 케이블 재연결
```

연결 확인:

```bash
pio device list
```

### 4. 다른 Linux / macOS / Windows

| OS | 참고 |
|----|------|
| Debian/Ubuntu | `sudo apt install build-essential git python3-pip libsdl2-dev pkg-config` · 시리얼 그룹 `dialout` |
| macOS | [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) · 시뮬: `brew install sdl2 pkg-config` ([sim/README.md](sim/README.md)) |
| Windows | PlatformIO IDE · 시뮬: MSYS2 + SDL2 ([sim/README.md](sim/README.md)) |

## UI

공통 **2단 방향키 내비**를 사용합니다. 문자 단축키 대신 화살표·Enter·Del 위주로 조작합니다.

- 상단 크롬 바: 앱 이름·페이지 점·배터리
- **G0** 버튼으로 M5Launcher로 돌아갑니다.

### 방향키

키캡에 인쇄된 화살표 키를 그대로 사용합니다. Fn 조합은 필요 없습니다.

| 키 | 방향 |
|----|------|
| `;` | 위 |
| `,` | 왼쪽 |
| `.` | 아래 |
| `/` | 오른쪽 |

### 공통 조작

| 동작 | 키 |
|------|-----|
| 항목 이동 | `↑` `↓` |
| 값 조절 / 하위 없음 | `←` `→` (앱에 따라) |
| 선택·실행 | `Enter` |
| 뒤로 | `←` 또는 `Del` |
| 앱 전환 | 목록 **맨 위**에서 `↑` → 크롬 포커스 → `←` `→` |
| 앱으로 복귀 | 크롬에서 `↓` 또는 `Enter` |

크롬이 포커스되면 바가 남색·제목이 노란색으로 강조됩니다.

## 기능

### Torch

액정을 최대한 밝게 켜 전등처럼 사용합니다.

| 동작 | 키 |
|------|-----|
| Bright / Time / Blink 포커스 | `↑` `↓` |
| 밝기·타임아웃 조절, Blink 토글 | `←` `→` |
| Blink 토글 (포커스 시) | `Enter` |
| 앱 전환 | Bright에서 `↑` → 크롬 |

- 타임아웃 조절 단위: **60초**
- 기본 최대 타임아웃: **10분(600초)**
- 타임아웃이 지나면 자동으로 꺼지고 다시 켜집니다.
- 장시간 고휘도 유지는 패널에 부담을 줄 수 있으니 타임아웃을 꼭 활용하세요.

### IR Reader / Sender

등록한 기기와 버튼의 IR 코드를 NVS에 저장하고 송신합니다.

**기기 목록**

- `↑` `↓` 로 기기 / `+ New` / `x Delete` 이동
- `Enter` — 기기 열기, New, Delete 실행
- 맨 위에서 `↑` — 크롬(앱 전환)

**기기 상세**

- `↑` `↓` 로 버튼 / `+ Learn` / `* Rename` / `x Delete` 이동
- `Enter` — 버튼 IR 송신 또는 액션 실행
- `←` 또는 `Del` — 기기 목록으로

**이름 입력 / 학습**

- `Enter` 저장, `Del` 한 글자 삭제, `←` 취소
- 학습 중 `←` 또는 `Del` 취소

IR 송신 핀: GPIO **44** (M5 공식 예제 기준)  
IR 수신 핀: GPIO **1** (`include/config.h`에서 변경 가능)

> Cardputer v1.x는 IR 수신 모듈이 별도일 수 있습니다. 수신이 안 되면 `kIrRxPin`을 실제 배선에 맞게 수정하세요.

## 데스크탑 시뮬레이터

Torch / IR UI를 PC에서 미리 보려면 `sim/` 환경을 사용합니다. Fedora에서는 위 **시스템 패키지**(`SDL2-devel`, `pkg-config` 등) 설치 후:

```bash
pio run -e sim -t upload
```

`upload` 타겟이 PC에서 SDL 창을 띄웁니다. 조작 키·구조·한계는 [sim/README.md](sim/README.md)를 참고하세요.

## 빌드

### M5Launcher 설치용 (권장)

기기에는 [M5Launcher](https://github.com/bmorcelli/Launcher)를 먼저 플래시해 두고, 이 프로젝트는 **OTA 앱 바이너리**로 설치합니다.

```bash
pio run -e m5launcher
```

빌드 결과:

| 파일 | 용도 |
|------|------|
| `dist/cardputer_adv_firmware.bin` | M5Launcher에 설치할 앱 이미지 |
| `.pio/build/m5launcher/firmware.bin` | 위와 동일 (원본 경로) |

**설치 방법**

1. M5Launcher 메뉴에서 `SD` / `OTA` / `WUI` 중 하나로 `.bin` 설치
2. 설치 후 목록에서 앱 실행

**런처로 돌아가기**

- 앱 실행 중 **G0 버튼**(키보드 왼쪽 상단) 한 번 → M5Launcher(factory)로 재부팅
- 복구: **G0 누른 채로 RST** → 런처 메뉴 강제 진입

> `firmware.bin`을 `esptool`로 `0x0`에 직접 쓰면 M5Launcher가 지워집니다. 반드시 M5Launcher의 Install 기능을 사용하세요.

### USB 직접 플래시 (단독 펌웨어)

M5Launcher 없이 단독으로 쓸 때만 사용합니다. Fedora에서 [USB 권한](#3-usb-업로드--시리얼-모니터)을 맞춘 뒤:

```bash
pio run -e m5stack-cardputer
pio run -e m5stack-cardputer -t upload
pio device monitor
```

업로드가 안 되면: 케이블이 **데이터** 지원인지, `pio device list`에 포트가 보이는지, `dialout` 그룹 적용 후 재로그인했는지 확인하세요. ESP32-S3은 부트로더 모드(보드의 **RST** 등)가 필요한 경우도 있습니다.

### 요구 사항

- Fedora: [개발 환경](#개발-환경-fedora) 절의 패키지 + [PlatformIO](https://platformio.org/)
- M5Cardputer / Cardputer-Adv 보드 (ESP32-S3)
- USB-C **데이터** 케이블

`platformio.ini`에 M5Cardputer, Arduino-IRremote 의존성이 정의되어 있습니다.

## 프로젝트 구조

```
.
├── platformio.ini
├── scripts/export_launcher_bin.py
├── dist/                     # M5Launcher용 .bin (빌드 후 생성)
├── include/config.h          # 핀, 제한값, 기본 설정
├── src/
│   ├── main.cpp
│   ├── app_shell.*           # 앱 전환 셸
│   ├── launcher_exit.*       # M5Launcher 복귀
│   └── apps/
│       ├── torch/            # 손전등 앱
│       └── ir/               # IR 앱 + NVS 저장소
└── README.md
```

## 설정

`include/config.h`에서 다음 값을 조정할 수 있습니다.

| 상수 | 기본값 | 설명 |
|------|--------|------|
| `kIrTxPin` | 44 | IR 송신 GPIO |
| `kIrRxPin` | 1 | IR 수신 GPIO |
| `kTorchDefaultTimeoutSec` | 600 | Torch 기본 타임아웃(초) |
| `kMaxDevices` | 16 | 저장 가능한 기기 수 |
| `kMaxButtonsPerDevice` | 16 | 기기당 버튼 수 |

## 로드맵

- [ ] 더 많은 유틸리티 앱
  - [ ] 시계
  - [ ] 타이머
  - [ ] 키보드 HID(블루투스, usb)
  - [ ] ssh commander: ssh로 명령을 전송하고 결과를 받아 출력
  - [ ] web commander: web protocol로 서버를 호출하고 결과를 받아 출력
- [ ] SD 카드 백업/복원

### 다음 major 버전

1. mobile(android) companion
  2. 스마트폰 알림 수신
     

## 라이선스

이 저장소의 라이선스는 별도 명시가 없으면 저장소 소유자 정책을 따릅니다.
