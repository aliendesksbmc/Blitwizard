
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

#ifndef BLITWIZARD_BLITWIZARDOBJECT_H_
#define BLITWIZARD_BLITWIZARDOBJECT_H_

#include <time.h>

#include "os.h"
#include "objectgraphicsdata.h"

#define VALIDATEMAGIC "489tjtgrniqrua9r"

struct blitwizardobject {
#ifdef VALIDATEBOBJ
#ifdef NDEBUG
#error "cannot validabe blitwizard object with NDEBUG defined"
#endif
    char validatemagic[] = VALIDATEBOBJ;
#endif
    char* respath;  // resource path
    int is3d;  // 0: 2d sprite with z-order value, 1: 3d mesh or sprite
    double x,y;  // 2d: x,y, 3d: x,y,z with z pointing up
    int deleted;  // 1: deleted (deletedobjects), 0: regular (objects)
    int refcount;  // refcount of luaidref references
    int doStepDone;  // used by luacfuncs_object_doAllSteps()
    int visible;  // 0: invisible, 1: visible (1 is default)

    // stuff we stored in the registry:
    char regTableName[64];  // registry table with custom user data
    char selfRefName[64];  // self ref

    // some event info:
    int haveDoAlways;
    int haveDoOften;
    int haveOnCollision;
    int haveOnMouseEnter;
    int haveOnMouseLeave;
    int haveOnMouseClick;
    int mouseWasReportedOnObject;  // for mouse enter/leave
    int invisibleToMouse;  // 1: invisible to mouse events, 0: normal

    union {
        struct {  // various stuff for 2d objects
            int textureFilter;  // 1: yes, 0: no
            struct {  // 2d flip info:
                int horiFlipped; // flipped horizontally
                int vertiFlipped; // flipped vertically
            };
        };
        struct {  // various things for 3d objects

        };
    };
    union {
        double z;
        int zindex;
    } vpos;
    union {
        struct {
            double x,y,z,r;
        } quaternion;
        double angle;
    } rotation;
    union {
        struct {
            struct {
                double x,y;
            } scale2d;
            double parallax;
        };
        struct {
            double x,y,z;
        } scale3d;
    };
    int scalechanged;
#ifdef USE_GRAPHICS
    struct objectgraphicsdata* graphics;
#endif
#if (defined(USE_PHYSICS2D) || defined(USE_PHYSICS3D))
    struct objectphysicsdata* physics;
#endif

    // list pointers for regular objects list:
    struct blitwizardobject* prev, *next;

    // list pointers for importantObjects list:
    struct blitwizardobject* importantPrev, *importantNext;

    // remember whether we already marked this object as important:
    int markedImportant;

    // temporarily disabled events
    // (due to errors in event handlers)
    time_t disabledDoAlways;
    time_t disabledOnCollision;
};

extern struct blitwizardobject* objects;
extern struct blitwizardobject* deletedobjects;

#endif  // BLITWIZARD_BLITWIZARDOBJECT_H_

