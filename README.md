# cardputer_adv_firmware

M5Stack Cardputer / Cardputer-Adv용 커스텀 펌웨어입니다. 기본 탑재 UI처럼 기능별로 좌우 전환하며, **Torch(손전등)** 와 **IR 리모컨** 기능을 제공합니다.

## UI

- 상단 크롬 바에 현재 앱 이름과 페이지 인디케이터를 표시합니다.
- **방향키** (`;` `,` `.` `/`)로 앱을 전환합니다 (`,` = 이전, `/` = 다음).
- **G0** 버튼으로 M5Launcher로 돌아갑니다.
- 각 앱은 독립 화면을 가지며, 앱 내부 조작은 앱별 단축키를 사용합니다.

### 방향키

키캡에 인쇄된 화살표 키를 그대로 사용합니다. Fn 조합은 필요 없습니다.

| 키 | 방향 |
|----|------|
| `;` | 위 |
| `,` | 왼쪽 |
| `.` | 아래 |
| `/` | 오른쪽 |

## 기능

### Torch

액정을 최대한 밝게 켜 전등처럼 사용합니다.

| 동작 | 키 |
|------|-----|
| 밝기 증가 | `+`, `=`, `]`, `k` |
| 밝기 감소 | `-`, `_`, `[`, `i` |
| 블링커 토글 | `b` |
| 타임아웃 조절 | `[` / `]` (60초 단위) |

- 기본 최대 타임아웃: **10분(600초)**
- 타임아웃이 지나면 자동으로 꺼지고 다시 켜집니다.
- 장시간 고휘도 유지는 패널에 부담을 줄 수 있으니 타임아웃을 꼭 활용하세요.

### IR Reader / Sender

등록한 기기와 버튼의 IR 코드를 NVS에 저장하고 송신합니다.

**기기 관리**

- 신규 기기 등록 (`N`)
- 기기 목록 조회 / 선택 (`;`/`.`, Enter)
- 기기 이름 수정 (`E`)
- 기기 삭제 (`Del`)

**버튼 관리**

- IR 수신(학습) (`R` — 기본 이름 `Button`, `N`으로 이름 입력 후 학습)
- 등록된 버튼 IR 송신 (`Enter` 또는 `S`)
- 버튼 이름 수정 (`M`)
- 버튼 삭제 (`Del`)

IR 송신 핀: GPIO **44** (M5 공식 예제 기준)  
IR 수신 핀: GPIO **1** (`include/config.h`에서 변경 가능)

> Cardputer v1.x는 IR 수신 모듈이 별도일 수 있습니다. 수신이 안 되면 `kIrRxPin`을 실제 배선에 맞게 수정하세요.

## 데스크탑 시뮬레이터

Torch / IR UI를 PC에서 미리 보려면 `sim/` 환경을 사용합니다.

```bash
# Linux: sudo apt install libsdl2-dev pkg-config
pio run -e sim -t upload
```

자세한 내용은 [sim/README.md](sim/README.md)를 참고하세요.

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

M5Launcher 없이 단독으로 쓸 때만 사용합니다.

```bash
pio run -e m5stack-cardputer
pio run -e m5stack-cardputer -t upload
pio device monitor
```

### 요구 사항

- [PlatformIO](https://platformio.org/)
- M5Cardputer 보드 (ESP32-S3)
- USB-C 데이터 케이블

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

- [ ] 추가 프로토콜 송신 지원 확대
- [ ] SD 카드 백업/복원
- [ ] 더 많은 유틸리티 앱 (타이머, 키보드 HID 등)

## 라이선스

이 저장소의 라이선스는 별도 명시가 없으면 저장소 소유자 정책을 따릅니다.
