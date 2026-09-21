# cardputer_adv_firmware

현재 릴리스: **[v0.3.1](https://github.com/bluedskim/cardputer_adv_firmware/releases/tag/v0.3.1)** · 개발 트리: `0.4.0-dev` (M1 BLE)

M5Stack **Cardputer-Adv** 전용 커스텀 펌웨어입니다. 기본 탑재 UI처럼 기능별로 좌우 전환하며, **Torch(손전등)**, **Timer**, **Stopwatch**, **Settings** 기능을 제공합니다.

주 개발 환경은 **Fedora** 기준이며, 아래 설치·업로드 절차도 Fedora를 우선으로 적어 두었습니다.

## 개발 환경 (Fedora)

### 1. 시스템 패키지

```bash
sudo dnf install git python3-pip gcc-c++ make SDL2-devel pkg-config
```


| 패키지                        | 용도                 |
| -------------------------- | ------------------ |
| `git`, `python3-pip`       | PlatformIO, 소스 클론  |
| `gcc-c++`, `make`          | 네이티브/sim 빌드        |
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

**방법 A —** `dialout` **그룹 (일반적)**

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


| OS            | 참고                                                                                                                                        |
| ------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| Debian/Ubuntu | `sudo apt install build-essential git python3-pip libsdl2-dev pkg-config` · 시리얼 그룹 `dialout`                                              |
| macOS         | [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) · 시뮬: `brew install sdl2 pkg-config` ([sim/README.md](sim/README.md)) |
| Windows       | PlatformIO IDE · 시뮬: MSYS2 + SDL2 ([sim/README.md](sim/README.md))                                                                        |




## UI

공통 **2단 방향키 내비**를 사용합니다. 문자 단축키 대신 화살표·Enter·Del 위주로 조작합니다.

- 상단 크롬 바: 앱 이름·페이지 점·배터리
- **G0** 버튼으로 M5Launcher로 돌아갑니다.



### 방향키

키캡에 인쇄된 화살표 키를 그대로 사용합니다. **Fn + 화살표**도 동일하게 동작합니다.


| 키             | 방향  |
| ------------- | --- |
| `;` 또는 Fn+`;` | 위   |
| `,` 또는 Fn+`,` | 왼쪽  |
| `.` 또는 Fn+`.` | 아래  |
| `/` 또는 Fn+`/` | 오른쪽 |




### 공통 조작


| 동작           | 키                                   |
| ------------ | ----------------------------------- |
| 항목 이동        | `↑` `↓`                             |
| 값 조절 / 하위 없음 | `←` `→` (앱에 따라)                     |
| 선택·실행        | `Enter`                             |
| 뒤로           | `←` 또는 `Del`                        |
| 앱 전환         | 목록 **맨 위**에서 `↑` → 크롬 포커스 → `←` `→` |
| 앱으로 복귀       | 크롬에서 `↓` 또는 `Enter`                 |


크롬이 포커스되면 바가 남색·제목이 노란색으로 강조됩니다.

## 기능



### Torch

액정을 최대한 밝게 켜 전등처럼 사용합니다.


| 동작                        | 키                 |
| ------------------------- | ----------------- |
| Bright / Time / Blink 포커스 | `↑` `↓`           |
| 밝기·타임아웃 조절, Blink 토글      | `←` `→`           |
| Blink 토글 (포커스 시)          | `Enter`           |
| 앱 전환                      | Bright에서 `↑` → 크롬 |


- 타임아웃 조절 단위: **60초** (최대 **60분**)
- 기본 타임아웃: **10분(600초)**
- 타임아웃이 지나면 자동으로 꺼졌다가 다시 켜집니다 (키를 누르면 바로 재시작).
- 장시간 고휘도 유지는 패널에 부담을 줄 수 있으니 타임아웃을 꼭 활용하세요.



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


| 항목           | `Enter`                 |
| ------------ | ----------------------- |
| Name         | 이름 입력                   |
| Start / Stop | 측정 시작·종료 (Stop 시 이력 저장) |
| History      | 저장된 기록 목록               |


- `↑` `↓` — 항목 이동 · 맨 위 `↑` → 크롬
- History 화면: `↑` `↓` 스크롤 · `←` / `Del` 복귀



### Settings

화면 순서상 **맨 마지막** 앱입니다. Sleep·밝기·BLE 링크를 다루고, 배터리·펌웨어 정보를 표시합니다.

- `↑` `↓` — Sleep / Bright / BLE 이동 · Sleep에서 `↑` → 크롬
- `←` `→` — Sleep: **Immediate** / 30초 / 1분 / **5분** / 10분 / **Never** · Bright: 32 / 64 / **128** / 192 / 255
  - Immediate: 곧 화면 OFF. **잠드는 순간** Sleep이 **30 sec**로 바뀌어 저장됨 (다시 Immediate 쓰려면 재선택)
- **BLE 행:** 상태 요약 표시 (`Off Unpaired` / `Off Paired` / `Pairing` / `Connecting` / `Connected` · sim은 `N/A`). `Enter` → BLE 서브화면
- **BLE 서브화면**
  - 상태·peer(있을 때) 표시
  - 문맥 동작만: 미페어 `Pair` · 페어됨 `Connect`/`Forget` · 진행 중 `Cancel` · 연결됨 `Disconnect`
  - Forget은 `Forget?` 확인 후 실행 (`Enter` 예 / `Del` 취소)
  - `←` / `Del` / 맨 위 `↑` → 메인 Settings (BLE 행 포커스)
- **Battery:** 잔량(%)과 전압(V)을 한 줄에 표시 (충전 여부는 헤더 배터리 아이콘으로 확인)
- **S/W Ver:** 펌웨어 버전 (`config.h`의 `kFirmwareVersion`)
- 입력이 없으면 Sleep 시간 후 백라이트 OFF. Timer/Stopwatch가 **실행 중이 아니고** BLE 라디오가 **꺼져 있으면** light sleep으로 추가 절전하고, **TCA8418 INT(GPIO11)** 로만 키 입력 시 복귀합니다. BLE Pair/Connect 중이거나 연결 중이면 light sleep하지 않으며, **60초** 무트래픽 후 자동 disconnect·radio off.
- Torch는 자체 밝기를 쓰며, 앱을 나가면 Settings Bright 값으로 복귀합니다.
- Android companion: [`companion/`](companion/) · 프로토콜: [`docs/ble_link.md`](docs/ble_link.md) · 검증: [`docs/m1_verify.md`](docs/m1_verify.md)



## 데스크탑 시뮬레이터

Torch / Timer / Stopwatch / Settings UI를 PC에서 미리 보려면 `sim/` 환경을 사용합니다. Fedora에서는 위 **시스템 패키지**(`SDL2-devel`, `pkg-config` 등) 설치 후:

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


| 파일                                   | 용도                    |
| ------------------------------------ | --------------------- |
| `dist/cardputer_adv_firmware.bin`    | M5Launcher에 설치할 앱 이미지 |
| `.pio/build/m5launcher/firmware.bin` | 위와 동일 (원본 경로)         |


**설치 방법**

1. M5Launcher 메뉴에서 `SD` / `OTA` / `WUI` 중 하나로 `.bin` 설치
2. 설치 후 목록에서 앱 실행

**런처로 돌아가기**

- 앱 실행 중 **G0 버튼**(키보드 왼쪽 상단) 한 번 → M5Launcher(factory)로 재부팅
- 복구: **G0 누른 채로 RST** → 런처 메뉴 강제 진입

> `firmware.bin`을 `esptool`로 `0x0`에 직접 쓰면 M5Launcher가 지워집니다. 반드시 M5Launcher의 Install 기능을 사용하세요.



### 실기 테스트 체크리스트

M5Launcher에 `.bin` 설치 후 기기에서 확인합니다. (sim은 백라이트 하드웨어가 없습니다.)

- [ ] **밝기:** Torch에서 Bright 포커스 후 `←` 시 패널이 어두워지고, `→` 시 밝아진다. 최대에서 `→`를 눌러도 255에 머문다.
- [ ] **타임아웃:** Time 포커스에서 `←` `→`가 60초 단위로 바뀌고, `Left`가 1초마다 줄어든다. 만료 후 메시지가 뜬 뒤 키가 다시 먹는다 (멈춘 것처럼 보이지 않음).
- [ ] **Timer:** `+ New`로 추가, Hours/Minutes만 조절, Start 후 카운트다운·DONE 표시.
- [ ] **Stopwatch:** Name 변경, Start/Stop 후 History에 기록(최대 10).
- [ ] **Settings:** Sleep·Bright 변경·저장, **BLE** 요약 행·서브화면(Pair/Connect/Cancel/Disconnect/Forget), 배터리 잔량·전압, 펌웨어 버전, 유휴 후 화면 꺼짐·키 복귀(이전 앱 유지), Timer 미실행·BLE off 시 light sleep.
- [ ] **BLE M1 (실기):** BLE → Pair → companion Scan → Connected → Disconnect → `Off Paired` · sleep/wake 앱 유지 · Forget 확인. 절차는 [companion/README.md](companion/README.md) · [docs/m1_verify.md](docs/m1_verify.md).
- [ ] **G0:** 앱 실행 중 G0 한 번 → M5Launcher로 돌아간다.
- [ ] **배터리:** 크롬 오른쪽에 아이콘이 보이고, USB 연결 시 번개 표시가 나타난다 (Cardputer는 CHG 핀이 없어 USB 연결로 추정).



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
- **Cardputer-Adv** 보드 (ESP32-S3, TCA8418 키보드)
- USB-C **데이터** 케이블

`platformio.ini`에 M5Cardputer 의존성이 정의되어 있습니다.

## 프로젝트 구조

```
.
├── platformio.ini
├── scripts/export_launcher_bin.py
├── dist/                     # M5Launcher용 .bin (빌드 후 생성)
├── docs/                     # ble_link.md(프로토콜) · m1/m2 검증 절차
├── companion/                # Android companion (M1/M2)
├── sim/                      # 데스크탑 시뮬레이터 (SDL2, Arduino/M5 스텁)
├── include/
│   ├── config.h
│   ├── system_settings.h     # Sleep/Bright 설정 (NVS)
│   └── ble/                  # 프로토콜 상수 · BleLink API
├── src/
│   ├── main.cpp
│   ├── app_shell.*           # 앱 전환 셸 · sleep/wake
│   ├── system_settings.cpp
│   ├── launcher_exit.*       # M5Launcher 복귀
│   ├── ble/ble_link.cpp      # NimBLE 링크 (sim stub)
│   ├── input/                # 키 edge 감지 · 방향키 매핑
│   ├── ui/                   # 배터리 아이콘
│   └── apps/
│       ├── torch/
│       ├── timer/
│       ├── stopwatch/
│       ├── web/              # HTTP commander (M2)
│       └── settings/
└── README.md
```



## 설정

`include/config.h`에서 다음 값을 조정할 수 있습니다.


| 상수                              | 기본값  | 설명                                 |
| ------------------------------- | ---- | ---------------------------------- |
| `kTorchDefaultTimeoutSec`       | 600  | Torch 기본 타임아웃(초)                   |
| `kTorchMaxTimeoutSec`           | 3600 | Torch 타임아웃 상한(초)                   |
| `kMaxTimers`                    | 8    | 저장 가능한 타이머 수                       |
| `kTimerMaxHours`                | 23   | 타이머 시간 상한                          |
| `kMaxStopwatchHistory`          | 10   | 스톱워치 이력 최대 건수                      |
| `kDefaultSleepOptionIndex`      | 3    | Sleep 기본값 (5분; Immediate·Never 포함) |
| `kDefaultBrightnessOptionIndex` | 2    | Bright 기본값 (128)                   |
| `kBleIdleDisconnectSec`         | 60   | BLE 연결/광고 무트래픽 후 radio off (초)     |




## 로드맵

- [ ] feature
  - [x] sleep
  - [ ] ~~Battery charge threshold 설정~~ (H/W가 미지원)
  - [ ] 설정 백업
  - [ ] 한글 출력

- [ ] 더 많은 유틸리티 앱
  - [ ] 녹음기
  - [ ] expression calculator
  - [ ] SD reader/writer
  - [ ] 메트로놈
  - [ ] tuner(기타줄 6개에 해당하는 음 재생)
  - [x] 타이머
  - [x] 스톱워치
  - [ ] 키보드 HID(블루투스, usb)
  - [ ] 시계
- [ ] SD 카드 백업/복원



### 다음 major 버전 — Phone-backed companion

테마: Cardputer는 얇은 단말 UI, Android 폰이 네트워크·보안·백그라운드를 맡는다.
Cardputer WiFi는 사용하지 않는다. 기기↔폰 링크는 **BLE**를 기본으로 한다.

목표: 기본 사용(오프라인 유틸)의 대기 수명은 현재 light sleep 수준을 유지한다.
배터리 부담은 **알림 상시 연결**이 크고, **커맨더 on-demand**는 상대적으로 작다.

#### 공통 (모든 기능의 선행 조건)

- [x] BLE 페어링·재연결·연결 상태 UI (Settings)
- [x] 링크 프로토콜: 프레임 타입, 요청/응답 ID, 에러 코드, 페이로드 크기 상한 ([docs/ble_link.md](docs/ble_link.md))
- [x] 시크릿은 폰 secure storage에만 보관 (Cardputer NVS에 SSH 키·토큰 저장 금지)
- [x] sleep/배터리 정책
  - [x] **기본 BLE off** — 페어링만 해 두고 라디오는 끈 상태가 기본 (현재와 비슷한 대기 수명)
  - [x] **커맨더 on-demand** — Web(또는 SSH) 요청 시 자동 Connect, 앱 종료·유휴 시 해제 *(Settings Connect는 선택)*
  - [ ] **알림 opt-in** — Settings 토글로만 켬. 끄면 연결·라디오 완전 분리 *(M4)*
  - [ ] 알림 ON이어도 화면은 기존 sleep 유지. 새 알림은 조용히 버퍼하고, 깨어난 뒤에만 목록 갱신 (강제 점등 wake는 옵션) *(M4)*
- [x] 비범위: iOS companion, Cardputer 자체 WiFi/SSH 스택, 풀 웹 브라우저



#### M1 — Companion 앱 (연결 기반)

- [x] Android companion 앱 ([`companion/`](companion/) · 연결·권한 안내)
- [x] 페어링·기본 BLE off / 연결 상태 표시
- [x] 비범위: 알림·커맨더 UI (아래 마일스톤)



#### M2 — Web commander (HTTP template)

- [x] 폰에 프리셋 저장: 이름, URL, method, 헤더/바디 슬롯
- [x] Cardputer에서 프리셋 선택 → 실행 → 응답 본문 페이징
- [x] 타임아웃·HTTP 에러·연결 끊김을 짧은 메시지로 표시
- [x] on-demand 연결만 사용 (대기 수명 유지)
- [x] 비범위: 임의 URL 브라우징, JS 렌더링, 쿠키 저널 UI

검증: [docs/m2_verify.md](docs/m2_verify.md) · 프로토콜: [docs/ble_link.md](docs/ble_link.md)

#### M3 — SSH commander (exec-only)

- [ ] 폰에 호스트 프리셋: host, user, 키/비밀번호(secure storage)
- [ ] 비대화형 exec: 한 줄 명령 → stdout/stderr 페이징
- [ ] known_hosts / 최초 연결 확인은 companion에서 처리
- [ ] on-demand 연결만 사용 (대기 수명 유지)
- [ ] 비범위: 인터랙티브 PTY, 포트포워드, SFTP, 멀티 세션 tmux



#### M4 — 알림 미러 (opt-in, 대기 비용 큼)

- [ ] Settings에서 알림 미러 ON/OFF (기본 OFF)
- [ ] 알림 접근 권한 + 읽기 전용 미러 (앱명/제목/요약)
- [ ] Cardputer 알림 목록·상세 (스크롤, `Del` 복귀)
- [ ] 수신 시 강제 화면 wake 없음 (버퍼 후 수동 확인; 점등 wake는 옵션)
- [ ] 비범위: 알림 액션(답장/닫기), 미디어 제어, 통화

우선순위: **공통 → M1 → M2 → M3 → M4**.
커맨더(M2/M3)를 알림(M4)보다 앞에 두어, 대기 배터리를 덜 깎는 기능을 먼저 출시한다.
기존 로드맵의 계산기·메트로놈·tuner·HID·시계는 오프라인 유틸로 유지하고, 이 major 축과 섞지 않는다.

## 디버깅

1. [ ] sleep에서 wake하였을 때 항상 torch화면으로 진입하는 듯. sleep시의 화면으로 돌아가야 함



## 라이선스

이 저장소의 라이선스는 별도 명시가 없으면 저장소 소유자 정책을 따릅니다.
