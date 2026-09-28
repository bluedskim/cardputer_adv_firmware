#include "launcher_exit.h"

#include <cstdio>

namespace launcher_exit {

void rebootToLauncher() {
    std::printf("[sim] G0 long: would return to M5Launcher on device\n");
}

}  // namespace launcher_exit
