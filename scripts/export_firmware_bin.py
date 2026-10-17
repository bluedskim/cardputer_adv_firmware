Import("env")

import os
import shutil
import subprocess


def _normalize_flash_freq_hz(frequency):
    frequency = str(frequency).replace("L", "")
    return str(int(int(frequency) / 1000000)) + "m"


def _flash_mode_for_merge(env):
    board = env.BoardConfig()
    memory_type = board.get(
        "build.arduino.memory_type",
        board.get("build.memory_type", ""),
    )
    if ["arduino"] == env.get("PIOFRAMEWORK") and memory_type in ("opi_opi", "opi_qspi"):
        return "dout"
    mode = env.subst("$BOARD_FLASH_MODE")
    if mode in ("qio", "qout"):
        return "dio"
    return mode


def _flash_freq_for_merge(env):
    board = env.BoardConfig()
    if "build.f_image" in board:
        return _normalize_flash_freq_hz(board.get("build.f_image"))
    return _normalize_flash_freq_hz(env.subst("$BOARD_F_FLASH"))


def _flash_size_for_merge(env):
    board = env.BoardConfig()
    return board.get("upload.flash_size", board.get("build.flash_size", "detect"))


def _esptool_path(env):
    platform = env.PioPlatform()
    pkg = platform.get_package_dir("tool-esptoolpy")
    for name in ("esptool", "esptool.py"):
        candidate = os.path.join(pkg, name)
        if os.path.isfile(candidate):
            return candidate
    raise RuntimeError("Could not find esptool in tool-esptoolpy package")


def _collect_flash_segments(env, firmware_bin):
    segments = []
    for offset, image_path in env.get("FLASH_EXTRA_IMAGES", []):
        path = env.subst(image_path)
        if not os.path.isfile(path):
            raise RuntimeError(f"Missing flash image for merge: {path}")
        segments.append((env.subst(offset), path))

    app_offset = env.subst("$ESP32_APP_OFFSET")
    if not os.path.isfile(firmware_bin):
        raise RuntimeError(f"Missing application image for merge: {firmware_bin}")
    segments.append((app_offset, firmware_bin))
    return segments


def export_firmware_binary(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    src = os.path.join(build_dir, "firmware.bin")
    dist_dir = os.path.join(project_dir, "dist")
    dst = os.path.join(dist_dir, "cardputer_adv_firmware.bin")
    merged_dst = os.path.join(dist_dir, "cardputer_adv_firmware-merged.bin")

    if not os.path.isfile(src):
        print(f"Firmware export skipped: missing {src}")
        return

    os.makedirs(dist_dir, exist_ok=True)
    shutil.copy2(src, dst)

    with open(src, "rb") as firmware:
        magic = firmware.read(1)
    if magic != b"\xe9":
        raise RuntimeError(f"Invalid ESP image magic in {src}: {magic!r}")

    board = env.BoardConfig()
    chip = board.get("build.mcu", "esp32")
    segments = _collect_flash_segments(env, src)

    merge_args = [
        env.subst("$PYTHONEXE"),
        _esptool_path(env),
        "--chip",
        chip,
        "merge_bin",
        "--flash_mode",
        _flash_mode_for_merge(env),
        "--flash_freq",
        _flash_freq_for_merge(env),
        "--flash_size",
        _flash_size_for_merge(env),
    ]
    for offset, path in segments:
        merge_args.extend([offset, path])
    merge_args.extend(["-o", merged_dst])

    print("Creating merged full-flash image:")
    print(" ", " ".join(merge_args))
    subprocess.run(merge_args, check=True)

    print(f"Firmware binary exported to {dst}")
    print(f"Merged full-flash image exported to {merged_dst}")


env.AddPostAction("$BUILD_DIR/firmware.bin", export_firmware_binary)
