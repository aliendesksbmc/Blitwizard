
/* blitwizard game engine - source code file

  Copyright (C) 2011-2013 Jonas Thiem

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

#ifndef BLITWIZARD_GRAPHICSTEXTURELIST_H_
#define BLITWIZARD_GRAPHICSTEXTURELIST_H_

#include "graphics.h"

// This file manages a linear texture list,
// and a hash map with texture file name -> texture list entry lookup.
//
// For loading a texture by path, you will want to do a hash map
// search by path using graphicstexturelist_GetTextureByName.
// 
// In case it isn't found, create it on the linear list using
// graphicstexturelist_AddTextureToList, then add it to the hash map
// using graphicstexturelist_AddTextureToHashmap.
//
// To iterate through all textures, use graphicstexturelist_DoForAllTextures.
//
// To delete all textures completely, use graphicstexturelist_FreeAllTextures.
// It calls graphicstexturelist_Destroy on all textures.

#ifdef USE_GRAPHICS

// This contains the cache info for one specific size of a texture:
struct graphicstexturescaled {
    struct graphicstexture* gt;  // NULL if not loaded
    int textureinhw;   // 1 if texture is in GPU memory, 0 if not
    char* diskcachepath;  // path to raw disk cache file or NULL
    size_t width,height;  // width/height of this particular scaled entry
};

// A managed texture entry containing all the different sized cached versions:
struct graphicstexturemanaged {
    char* path;  // original texture path this represents
    struct graphicstexturescaled* scalelist;  // one dimensional array
    int scalelistcount;  // count of scalelist array items
    int origscale;  // array index of scalelist of item scaled in original size

    // initialise to zeros and then don't touch:
    struct graphicstexturemanaged* next;
    struct graphicstexturemanaged* hashbucketnext;
};

// Find a texture by doing a hash map lookup:
struct graphicstexturemanaged* graphicstexturelist_GetTextureByName
(const char* name);

// Add a texture to the hash map:
void graphicstexturelist_AddTextureToHashmap(
struct graphicstexturemanaged* gt);

// Remove a texture from the hash map:
void graphicstexturelist_RemoveTextureFromHashmap(
struct graphicstexturemanaged* gt);

// get all textures out of GPU memory:
void graphicstexturelist_TransferTexturesFromHW();

// get previous texture in texture list:
struct graphicstexturemanaged* graphicstexturelist_GetPreviousTexture(
struct graphicstexturemanaged* gt);

// add a new empty graphics texture with the given file path
// set to the list and return it:
struct graphicstexturemanaged* graphicstexturelist_AddTextureToList(
const char* path);

// Remove a texture from the linear list (it might still be in the hash map):
void graphicstexturelist_RemoveTextureFromList(
struct graphicstexturemanaged* gt, struct graphicstexturemanaged* prev);

// Destroy a texture. It will be removed from the linear list and the
// hash map, then all its disk cache entries of the graphicstexturescaled
// list will be diskcache_Delete()'d, the textures will be
// graphicstexture_Destroy()'d, and the char* struct members will be free'd.
void graphicstexturelist_DestroyTexture(struct graphicstexturemanaged* gt);

// Calls graphicstexturelist_DestroyTexture on all textures.
int graphicstexturelist_FreeAllTextures();

// Do something with all textures:
void graphicstexturelist_DoForAllTextures(
int (*callback)(struct graphicstexturemanaged* texture,
struct graphicstexturemanaged* previoustexture, void* userdata),
void* userdata);

#endif  // USE_GRAPHICS

#endif  // BLITWIZARD_GRAPHICSTEXTURELIST_H_

