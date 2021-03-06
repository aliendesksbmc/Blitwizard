
/* blitwizard game engine - source code file

  Copyright (C) 2011-2014 Jonas Thiem

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

*/

#include "config.h"
#include "os.h"

#ifdef USE_GRAPHICS

// various standard headers
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef WINDOWS
#include <windows.h>
#endif
#include <stdarg.h>

#include "logging.h"
#ifdef USE_SDL_GRAPHICS
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#endif
#include "graphicstexture.h"
#include "graphics.h"
#include "graphicstexturelist.h"
#include "diskcache.h"
#include "imgloader.h"
#include "timefuncs.h"
#include "hash.h"
#include "file.h"
#ifdef NOTHREADEDSDLRW
#include "main.h"
#endif
#include "threading.h"

static struct graphicstexturemanaged *texlist = NULL;
hashmap* texhashmap = NULL;
static mutex* listMutex = NULL;

// this runs on application start:
__attribute__((constructor)) static void graphicstexturelist_init(void) {
    listMutex = mutex_create();
}

void graphicstexturelist_initializeHashmap(void) {
    if (texhashmap) {
        return;
    }
    texhashmap = hashmap_new(1024 * 1024);
}

struct graphicstexturemanaged *graphicstexturelist_addTextureToList(
        const char* path) {
    struct graphicstexturemanaged *m = malloc(sizeof(*m));
    if (!m) {
        return NULL;
    }
    memset(m, 0, sizeof(*m));
    m->path = strdup(path);
    if (!m->path) {
        free(m);
        return NULL;
    }
    m->preferredSize = -1;
    m->origscale = -1;
    mutex_lock(listMutex);
    m->next = texlist;
    texlist = m;
    mutex_release(listMutex);
    return m;
}

void graphicstexturelist_removeTextureFromList(
        struct graphicstexturemanaged *li,
        struct graphicstexturemanaged *prev) {
    mutex_lock(listMutex);
    if (prev) {
        prev->next = li->next;
    } else {
        texlist = li->next;
    }
    mutex_release(listMutex);
}

struct graphicstexturemanaged *graphicstexturelist_getTextureByName(
        const char* name) {
    mutex_lock(listMutex);
    graphicstexturelist_initializeHashmap();
    uint32_t i = hashmap_getIndex(texhashmap, name, strlen(name), 1);
    struct graphicstexturemanaged *m =
        (struct graphicstexturemanaged*)(texhashmap->items[i]);
    while (m && !(strcasecmp(m->path, name) == 0)) {
        m = m->hashbucketnext;
    }
    mutex_release(listMutex);
    return m;
}

void graphicstexturelist_addTextureToHashmap(
        struct graphicstexturemanaged *m) {
    mutex_lock(listMutex);
    graphicstexturelist_initializeHashmap();
    uint32_t i = hashmap_getIndex(texhashmap, m->path, strlen(m->path), 1);
    
    m->hashbucketnext = (struct graphicstexturemanaged*)(texhashmap->items[i]);
    texhashmap->items[i] = m;
    mutex_release(listMutex);
}

void graphicstexturelist_removeTextureFromHashmap(
        struct graphicstexturemanaged *gt) {
    mutex_lock(listMutex);
    graphicstexturelist_initializeHashmap();
    uint32_t i = hashmap_getIndex(texhashmap, gt->path, strlen(gt->path), 1);
    struct graphicstexturemanaged *gt2 =
        (struct graphicstexturemanaged*)(texhashmap->items[i]);
    struct graphicstexturemanaged *gtprev = NULL;
    while (gt2) {
        if (gt2 == gt) {
            if (gtprev) {
                gtprev->next = gt->hashbucketnext;
            } else {
                texhashmap->items[i] = gt->hashbucketnext;
            }
            gt->hashbucketnext = NULL;
            mutex_release(listMutex);
            return;
        }

        gtprev = gt2;
        gt2 = gt2->hashbucketnext;
    }
    mutex_release(listMutex);
}

void graphicstexturelist_transferTextureFromHW_internal(
        struct graphicstexturemanaged *gt) {
    int i = 0;
    while (i < gt->scalelistcount) {
        struct graphicstexturescaled* s = &gt->scalelist[i];
        if (!s->pixels) {  // no pixels stored in system memory
            // if we can, download texture from GPU:
            if (s->gt) {
                void* newpixels = malloc(s->width * s->height * 4);
                if (newpixels) {
                    int r = graphicstexture_pixelsFromTexture(s->gt,
                        newpixels);
                    if (r) {
                        s->pixels = newpixels;
                    } else {
                        free(newpixels);
                    }
                }
            }
        }
        if (s->gt) {
            // destroy texture from the GPU
            graphicstexture_destroy(s->gt);
            s->gt = NULL;
        }
        i++;
    }
}

void graphicstexturelist_transferTextureFromHW(
struct graphicstexturemanaged *gt) {
    mutex_lock(listMutex);
    graphicstexturelist_transferTextureFromHW_internal(gt);
    mutex_release(listMutex);
}

static void graphicstexturelist_invalidateTextureInHW_internal(
        struct graphicstexturemanaged *gt) {
    int i = 0;
    while (i < gt->scalelistcount) {
        struct graphicstexturescaled* s = &gt->scalelist[i];
        if (s->gt) {
            // destroy texture from the GPU
            graphicstexture_destroy(s->gt);
            s->gt = NULL;
        }
        s->refcount = 0;
        i++;
    }
}

void graphicstexturelist_invalidateTextureInHW(
        struct graphicstexturemanaged *gt) {
    mutex_lock(listMutex);
    graphicstexturelist_invalidateTextureInHW_internal(
    gt);
    mutex_release(listMutex);
}

void graphicstexturelist_transferTexturesFromHW(void) {
    mutex_lock(listMutex);
    struct graphicstexturemanaged *gt = texlist;
    while (gt) {
        graphicstexturelist_transferTextureFromHW_internal(gt);
        gt = gt->next;
    }
    mutex_release(listMutex);
}

void graphicstexturelist_invalidateHWTextures(void) {
    mutex_lock(listMutex);
    struct graphicstexturemanaged *gt = texlist;
    while (gt) {
        graphicstexturelist_invalidateTextureInHW_internal(gt);
        gt = gt->next;
    }
    mutex_release(listMutex);
}

void graphicstexturelist_transferTextureToHW(
        struct graphicstexturemanaged *gt) {
    mutex_lock(listMutex);
    int i = 0;
    while (i < gt->scalelistcount) {
        struct graphicstexturescaled* s = &gt->scalelist[i];
        if (!s->pixels) {  // no pixels stored in system memory
            // if we can, download texture from GPU:
            if (s->gt) {
                void* newpixels = malloc(s->width * s->height * 4);
                if (newpixels) {
                    int r = graphicstexture_pixelsFromTexture(s->gt,
                        newpixels);
                    if (r) {
                        s->pixels = newpixels;
                    } else {
                        free(newpixels);
                    }
                }
            }
        }
        if (s->gt) {
            // destroy texture from the GPU
            graphicstexture_destroy(s->gt);
            s->gt = NULL;
        }
        i++;
    }
    mutex_release(listMutex);
}

void graphicstexturelist_destroyTexture(
        struct graphicstexturemanaged *gt) {
    int i = 0;
    while (i < gt->scalelistcount) {
        struct graphicstexturescaled* s = &gt->scalelist[i];
        if (s->gt) {
            // destroy texture from the GPU
            graphicstexture_destroy(s->gt);
        }
        if (s->pixels) {
            // destroy texture in memory
            free(s->pixels);
        }
        if (s->diskcachepath) {
            // destroy texture from disk cache
            diskcache_delete(s->diskcachepath);
            free(s->diskcachepath);
        }
        i++;
    }
    free(gt->scalelist);
    free(gt);
}

void graphicstexturelist_freeAllTextures(void) {
    // free all textures
    mutex_lock(listMutex);
    struct graphicstexturemanaged *gt = texlist;
    while (gt) {
        struct graphicstexturemanaged *gtnext = gt->next;
        graphicstexturelist_destroyTexture(gt);
        gt = gtnext;
    }
    mutex_release(listMutex);
}

void graphicstexturelist_doForAllTextures(
int (*callback)(struct graphicstexturemanaged *texture,
struct graphicstexturemanaged *previoustexture,
void* userdata), void* userdata) {
    mutex_lock(listMutex);
    struct graphicstexturemanaged *gt = texlist;
    struct graphicstexturemanaged *gtprev = NULL;
    while (gt) {
        struct graphicstexturemanaged *gtnext = gt->next;
        if (callback(gt, gtprev, userdata)) {
            // entry is still valid (callback return 1), remember it as prev
            gtprev = gt;
        }
        gt = gtnext;
    }
    mutex_release(listMutex);
}

#endif // ifdef USE_GRAPHICS

