#include "modules/overlay/overlay.h"
#include <cstdio>

auto main() -> int {
    std::printf("guard esp\n");
    Overlay overlay;
    if (!overlay.create()) {
        std::printf("overlay failed\n");
        return 1;
    }
    Game game;
    overlay.run(game);
    overlay.destroy();
    return 0;
}
