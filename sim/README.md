# Desktop Simulator (`sim/`)

Torch / Timer / Stopwatch / Settings UI를 PC에서 SDL2 창으로 미리 확인합니다. 실제 키보드 하드웨어는 스텁으로 대체됩니다.

PlatformIO 설치·USB 권한 등 공통 개발 환경은 저장소 루트 [README.md](../README.md#개발-환경-fedora)의 **Fedora** 절을 따르세요.

## 추가 패키지 (SDL2)

루트 README의 Fedora `dnf install` 한 번으로 sim 빌드에 필요한 `SDL2-devel`, `pkg-config`가 포함됩니다. 다른 OS만 아래를 참고하세요.

### Linux (Debian/Ubuntu)

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

공통 2단 내비 (실기와 동일):

| 키 | 동작 |
|----|------|
| `↑` `↓` (`;` `.`) | 항목 이동 |
| `←` `→` (`,` `/`) | 값 조절(Torch) / 뒤로(`←`) |
| `Enter` | 선택·실행 |
| `Del` | 뒤로 또는 글자 삭제 |
| 맨 위에서 `↑` | 크롬 포커스 → `←` `→` 로 앱 전환 (실기는 Fn+화살표도 동일) |
| 크롬에서 `↓` / `Enter` | 앱으로 복귀 |
| `` ` `` | G0 (시뮬에서는 콘솔 메시지) |

## 구조

```
sim/
  include/            # Arduino / M5Cardputer / Preferences 스텁
  src/                # SDL main, 키보드 폴링
src/apps/             # 실제 앱 코드 (sim 빌드에서 재사용)
```

## 한계

- M5Launcher / NVS 영속성은 메모리 스텁
- BLE는 stub (`BLE N/A`); 서브화면만 열리고 동작은 no-op
- 실기 최종 확인은 여전히 필요 ([docs/m1_verify.md](../docs/m1_verify.md))
