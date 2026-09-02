# cardputer_adv_firmware

현재 릴리스: **[v0.2.1](https://github.com/bluedskim/cardputer_adv_firmware/releases/tag/v0.2.1)**

M5Stack Cardputer / Cardputer-Adv용 커스텀 펌웨어입니다. 기본 탑재 UI처럼 기능별로 좌우 전환하며, **Torch(손전등)**, **Timer**, **Stopwatch**, **IR 리모컨**, **Settings** 기능을 제공합니다.

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

키캡에 인쇄된 화살표 키를 그대로 사용합니다. **Fn + 화살표**도 동일하게 동작합니다.

| 키 | 방향 |
|----|------|
| `;` 또는 Fn+`;` | 위 |
| `,` 또는 Fn+`,` | 왼쪽 |
| `.` 또는 Fn+`.` | 아래 |
| `/` 또는 Fn+`/` | 오른쪽 |

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

- 타임아웃 조절 단위: **60초** (최대 **60분**)
- 기본 타임아웃: **10분(600초)**
- 타임아웃이 지나면 자동으로 꺼졌다가 다시 켜집니다 (키를 누르면 바로 재시작).
- 장시간 고휘도 유지는 패널에 부담을 줄 수 있으니 타임아웃을 꼭 활용하세요.

### IR Reader / Sender

등록한 기기와 버튼의 IR 코드를 NVS에 저장하고 송신합니다.

**기기 목록**

- `↑` `↓` 로 기기 / `+ New` 이동 (목록이 길면 스크롤)
- `Enter` — 기기 열기 또는 New
- 맨 위에서 `↑` — 크롬(앱 전환)

**기기 상세**

- `↑` `↓` 로 버튼 / `+ Learn` / `* Dev name` / `* Rename` / `x Delete` / `x Remove` 이동
- `Enter` — 버튼 IR 송신 또는 액션 실행
- `←` 또는 `Del` — 기기 목록으로
- `x Delete`는 버튼, `x Remove`는 기기 삭제 (`Ent yes` / `< no` 확인)

**이름 입력 / 학습**

- `Enter` 저장, `Del` 한 글자 삭제, `←` 취소
- 학습 중 `←` 또는 `Del` 취소

IR 송신 핀: GPIO **44** (M5 공식 예제 기준)  
IR 수신 핀: GPIO **1** (`include/config.h`에서 변경 가능)

> Cardputer v1.x는 IR 수신 모듈이 별도일 수 있습니다. 수신이 안 되면 `kIrRxPin`을 실제 배선에 맞게 수정하세요.

### Timer

여러 개의 카운트다운 타이머를 NVS에 저장합니다. **시간·분** 단위로만 설정합니다.

**목록**

- `↑` `↓` — 타이머 / `+ New`
- `Enter` — 타이머 열기·시작·완료 해제 / New 추가
- 실행 중 `Enter` — 취소
- 맨 위 `↑` — 크롬(앱 전환)

**설정**

- `↑` `↓` — Hours / Minutes / `> Start` / `x Remove`
- `←` `→` — 시간·분 조절 (0–23h, 0–59m)
- `Enter` — Start 또는 Remove(확인)
- `←` / `Del` — 목록으로

완료된 타이머는 목록에 `DONE`으로 표시됩니다.

### Stopwatch

이름을 지정하고 측정한 기록을 NVS에 저장합니다 (최대 **10**건).

| 항목 | `Enter` |
|------|---------|
| Name | 이름 입력 |
| Start / Stop | 측정 시작·종료 (Stop 시 이력 저장) |
| History | 저장된 기록 목록 |

- `↑` `↓` — 항목 이동 · 맨 위 `↑` → 크롬
- History 화면: `↑` `↓` 스크롤 · `←` / `Del` 복귀

### Settings

화면 순서상 **맨 마지막** 앱입니다. 유휴 시 화면을 끄는 **Sleep** 시간을 NVS에 저장합니다.

- `↑` `↓` — 항목 이동 · 맨 위 `↑` → 크롬
- `←` `→` — Sleep 시간 변경 (30초 / 1분 / **5분** / 10분, 기본 5분)
- 입력이 없으면 설정된 시간 후 백라이트가 꺼지며, 아무 키나 누르면 복귀합니다. 이때 **Timer**·**Stopwatch**는 계속 동작합니다.

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

### 실기 테스트 체크리스트

M5Launcher에 `.bin` 설치 후 기기에서 확인합니다. (sim은 백라이트·IR 하드웨어가 없습니다.)

- [ ] **밝기:** Torch에서 Bright 포커스 후 `←` 시 패널이 어두워지고, `→` 시 밝아진다. 최대에서 `→`를 눌러도 255에 머문다.
- [ ] **타임아웃:** Time 포커스에서 `←` `→`가 60초 단위로 바뀌고, `Left`가 1초마다 줄어든다. 만료 후 메시지가 뜬 뒤 키가 다시 먹는다 (멈춘 것처럼 보이지 않음).
- [ ] **IR 학습:** 상세에서 `+ Learn` → 리모컨 버튼을 누르면 저장된다. 안 되면 `kIrRxPin`(기본 GPIO 1)을 배선에 맞게 바꾼다.
- [ ] **IR 송신:** 저장된 버튼에서 `Enter` 시 기기가 반응한다 (GPIO 44).
- [ ] **IR 목록:** 기기가 많을 때 `↓`로 화면 밖 항목까지 스크롤된다.
- [ ] **IR 상세:** `* Dev name`으로 기기 이름이 바뀐다. `x Remove`로 기기가 지워지고, `x Delete`로 버튼이 지워진다(확인 후).
- [ ] **Timer:** `+ New`로 추가, Hours/Minutes만 조절, Start 후 카운트다운·DONE 표시.
- [ ] **Stopwatch:** Name 변경, Start/Stop 후 History에 기록(최대 10).
- [ ] **Settings:** Sleep 시간 변경·저장, 유휴 후 화면 꺼짐·키 입력 시 복귀.
- [ ] **G0:** 앱 실행 중 G0 한 번 → M5Launcher로 돌아간다.
- [ ] **배터리:** 크롬 오른쪽에 %가 보이고, USB 연결 시 충전 표시가 바뀐다.

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
│       ├── timer/            # 타이머 앱 + NVS
│       ├── stopwatch/        # 스톱워치 앱 + NVS
│       ├── ir/               # IR 앱 + NVS 저장소
│       └── settings/         # 설정 앱 + 시스템 NVS
└── README.md
```

## 설정

`include/config.h`에서 다음 값을 조정할 수 있습니다.

| 상수 | 기본값 | 설명 |
|------|--------|------|
| `kIrTxPin` | 44 | IR 송신 GPIO |
| `kIrRxPin` | 1 | IR 수신 GPIO |
| `kTorchDefaultTimeoutSec` | 600 | Torch 기본 타임아웃(초) |
| `kTorchMaxTimeoutSec` | 3600 | Torch 타임아웃 상한(초) |
| `kMaxDevices` | 16 | 저장 가능한 IR 기기 수 |
| `kMaxButtonsPerDevice` | 16 | IR 기기당 버튼 수 |
| `kMaxTimers` | 8 | 저장 가능한 타이머 수 |
| `kTimerMaxHours` | 23 | 타이머 시간 상한 |
| `kMaxStopwatchHistory` | 10 | 스톱워치 이력 최대 건수 |
| `kDefaultSleepOptionIndex` | 2 | Sleep 기본값 (5분) |

## 로드맵

- [ ] feature
  - [x] sleep
  - [ ] 설정 저장

- [ ] 더 많은 유틸리티 앱
  - [ ] SD reader/writer
  - [ ] 시계
  - [ ] 메트로놈
  - [ ] tuner(기타줄 6개에 해당하는 음 재생)
  - [x] 타이머
  - [x] 스톱워치
  - [ ] 키보드 HID(블루투스, usb)
  - [ ] ssh commander: ssh로 명령을 전송하고 결과를 받아 출력
  - [ ] web commander: web protocol로 서버를 호출하고 결과를 받아 출력
- [ ] SD 카드 백업/복원

### 다음 major 버전

1. mobile(android) companion
  2. 스마트폰 알림 수신
     

## 라이선스

이 저장소의 라이선스는 별도 명시가 없으면 저장소 소유자 정책을 따릅니다.
