Import("env")

import os
import shutil


def export_launcher_binary(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    src = os.path.join(build_dir, "firmware.bin")
    dist_dir = os.path.join(project_dir, "dist")
    dst = os.path.join(dist_dir, "cardputer_adv_firmware.bin")

    if not os.path.isfile(src):
        print(f"Launcher export skipped: missing {src}")
        return

    os.makedirs(dist_dir, exist_ok=True)
    shutil.copy2(src, dst)

    with open(src, "rb") as firmware:
        magic = firmware.read(1)
    if magic != b"\xe9":
        raise RuntimeError(f"Invalid ESP image magic in {src}: {magic!r}")

    print(f"M5Launcher binary exported to {dst}")


env.AddPostAction("$BUILD_DIR/firmware.bin", export_launcher_binary)
