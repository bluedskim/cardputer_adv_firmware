# Desktop Simulator (`sim/`)

Torch / IR UI를 PC에서 SDL2 창으로 미리 확인합니다. 실제 IR·키보드 하드웨어는 스텁으로 대체됩니다.

## 요구 사항

### Linux (Ubuntu/Debian)

```bash
sudo apt install build-essential libsdl2-dev pkg-config
```

### macOS

```bash
brew install sdl2 pkg-config
```

`platformio.ini`의 `native_arm` 환경을 사용하려면 macOS용 플래그를 추가하세요.

### Windows

[MSYS2 UCRT64](https://www.msys2.org/) + SDL2 개발 패키지가 필요합니다. [M5GFX PlatformIO_SDL 가이드](https://github.com/m5stack/M5GFX/tree/master/examples/PlatformIO_SDL)를 참고하세요.

## 실행

저장소 루트에서:

```bash
pio run -e sim -t upload
```

`upload` 타겟이 PC에서 시뮬레이터 실행 파일을 띄웁니다.

빌드만:

```bash
pio run -e sim
.pio/build/sim/program
```

## 조작 키

| 키 | 동작 |
|----|------|
| `,` `/` 또는 ← → | 앱 전환 |
| `;` `.` 또는 ↑ ↓ | IR 목록 이동 |
| `B` | Torch 블링커 |
| `+` `-` `[` `]` | Torch 밝기/타임아웃 |
| `` ` `` | G0 (시뮬에서는 콘솔 메시지) |
| `N` `R` `Enter` `Del` | IR 앱 조작 |
| `I` | IR 학습 화면에서 테스트 IR 코드 주입 |

## 구조

```
sim/
  include/            # Arduino / M5Cardputer / IR / Preferences 스텁
  src/                # SDL main, 키보드 폴링
src/apps/             # 실제 앱 코드 (sim 빌드에서 재사용)
```

## 한계

- M5Launcher / NVS 영속성은 메모리 스텁
- IR은 송신 로그만, 수신은 `I` 키로 테스트 데이터 주입
- 실기 최종 확인은 여전히 필요
