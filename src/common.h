
#ifndef SLS_COMMON_H
#define SLS_COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "slsmacros.h"
#include "results.h"

#ifdef __EMSCRIPTEN__
#include <SDL2/SDL_opengles2.h>
#include <emscripten.h>
#include <SDL2/SDL.h>

#else
#include <GL/glew.h>

#include <SDL.h>
#endif

#endif // !SLS_COMMON_H
