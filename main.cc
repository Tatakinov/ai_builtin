#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_main.h>

#include "ai.h"
#include "logger.h"

int main(int argc, char **argv) {
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "io.github.tatakinov.ninix-kagari.ai_builtin");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return 1;
    }
    atexit(SDL_Quit);
    if (!TTF_Init()) {
        return 1;
    }
    atexit(TTF_Quit);

    Ai ai;

#if defined(DEBUG)
    ai.create(0);
    ai.show(0);
    ai.setSurface(0, 0);
#endif // DEBUG

    while (ai) {
        ai.run();
    }

	return 0;
}
