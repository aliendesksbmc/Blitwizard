
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

/// Blitwizard namespace containing the generic
// @{blitwizard.object|blitwizard game entity object} and various sub
// namespaces for @{blitwizard.physics|physics},
// @{blitwizard.graphics|graphics} and more.
// @author Jonas Thiem  (jonas.thiem@gmail.com)
// @copyright 2011-2013
// @license zlib
// @module blitwizard

#include <stdlib.h>
#include <string.h>

#include "os.h"
#ifdef USE_SDL_GRAPHICS
#include "SDL.h"
#endif
#include "graphicstexture.h"
#include "graphics.h"
#include "luaheader.h"
#include "luastate.h"
#include "luaerror.h"
#include "luafuncs.h"
#include "blitwizardobject.h"
#include "luafuncs_object.h"
#include "luafuncs_objectgraphics.h"
#include "luafuncs_objectphysics.h"

struct blitwizardobject* objects = NULL;
struct blitwizardobject* deletedobjects = NULL;

/// Blitwizard object which represents an 'entity' in the game world
// with visual representation, behaviour code and collision shape.
// @type object

void cleanupobject(struct blitwizardobject* o) {
    // clear up all the graphics, physics, event function things
    // attached to the object:
    luafuncs_objectgraphics_unload(o);
#if (defined(USE_PHYSICS2D) || defined(USE_PHYSICS3D))
    if (o->physics) {
        luafuncs_freeObjectPhysicsData(o->physics);
        o->physics = NULL;
    }
#endif
    luacfuncs_object_clearRegistryTable(luastate_GetStatePtr(), o);
}

static int garbagecollect_blitwizobjref(lua_State* l) {
    // we need to decrease our reference count of the
    // blitwizard object we referenced to.

    // get id reference to object
    struct luaidref* idref = lua_touserdata(l, -1);

    if (!idref || idref->magic != IDREF_MAGIC
    || idref->type != IDREF_BLITWIZARDOBJECT) {
        // either wrong magic (-> not a luaidref) or not a blitwizard object
        lua_pushstring(l, "internal error: invalid blitwizard object ref");
        lua_error(l);
        return 0;
    }

    // it is a valid blitwizard object, decrease ref count:
    struct blitwizardobject* o = idref->ref.bobj;
    o->refcount--;

    // if it's already deleted and ref count is zero, remove it
    // entirely and free it:
    if (o->deleted && o->refcount <= 0) {
        cleanupobject(o);

        // remove object from the list
        if (o->prev) {
            // adjust prev object to have new next pointer
            o->prev->next = o->next;
        } else {
            // was first object in deleted list -> adjust list start pointer
            deletedobjects = o->next;
        }
        if (o->next) {
            // adjust next object to have new prev pointer
            o->next->prev = o->prev;
        }

        // free object
        free(o);
    }
    return 0;
}

void luacfuncs_object_obtainRegistryTable(lua_State* l,
struct blitwizardobject* o);

void luacfuncs_pushbobjidref(lua_State* l, struct blitwizardobject* o) {
    // create luaidref userdata struct which points to the blitwizard object
    struct luaidref* ref = lua_newuserdata(l, sizeof(*ref));
    memset(ref, 0, sizeof(*ref));
    ref->magic = IDREF_MAGIC;
    ref->type = IDREF_BLITWIZARDOBJECT;
    ref->ref.bobj = o;

    // set garbage collect callback:
    luastate_SetGCCallback(l, -1, (int (*)(void*))&garbagecollect_blitwizobjref);

    // set metatable __index and __newindex to registry table:
    lua_getmetatable(l, -1);
    lua_pushstring(l, "__index");
    luacfuncs_object_obtainRegistryTable(l, o);
    lua_settable(l, -3);
    lua_pushstring(l, "__newindex");
    luacfuncs_object_obtainRegistryTable(l, o);
    lua_settable(l, -3);
    lua_setmetatable(l, -2);

    o->refcount++;
}

struct blitwizardobject* toblitwizardobject(lua_State* l, int index, int arg, const char* func) {
    if (lua_type(l, index) != LUA_TUSERDATA) {
        haveluaerror(l, badargument1, arg, func, "blitwizard object", lua_strtype(l, index));
    }
    if (lua_rawlen(l, index) != sizeof(struct luaidref)) {
        haveluaerror(l, badargument2, arg, func, "not a valid blitwizard object");
    }
    struct luaidref* idref = lua_touserdata(l, index);
    if (!idref || idref->magic != IDREF_MAGIC
    || idref->type != IDREF_BLITWIZARDOBJECT) {
        haveluaerror(l, badargument2, arg, func, "not a valid blitwizard object");
    }
    struct blitwizardobject* o = idref->ref.bobj;
    if (o->deleted) {
        haveluaerror(l, badargument2, arg, func, "this blitwizard object was deleted");
    }
    return o;
}

/// Create a new blitwizard object which is represented as a 2d or
// 3d object in the game world.
// 2d objects are on a separate 2d plane, and 3d objects are inside
// the 3d world.
// Objects can have behaviour and collision info attached and move
// around. They are what eventually makes the action in your game!
// @function new
// @tparam boolean 3d specify true if you wish this object to be a 3d object, or false if you want it to be a flat 2d object
// @tparam string resource (optional) if you specify the file path to a resource here (optional), this resource will be loaded and used as a visual representation for the object. The resource must be a supported graphical object, e.g. an image (.png) or a 3d model (.mesh). You can also specify nil here if you don't want any resource to be used.
// @treturn userdata Returns a @{blitwizard.object|blitwizard object}
int luafuncs_object_new(lua_State* l) {
    // technical first argument is the object table,
    // which we don't care about in the :new function.
    // actual specified first argument is the second one
    // on the lua stack.

    // first argument needs to be 2d/3d boolean:
    if (lua_type(l, 2) != LUA_TBOOLEAN) {
        return haveluaerror(l, badargument1, 1, "blitwizard.object:new",
        "boolean", lua_strtype(l, 2));
    }
    int is3d = lua_toboolean(l, 2);

    // second argument, if present, needs to be the resource:
    const char* resource = NULL;
    if (lua_gettop(l) >= 3 && lua_type(l, 3) != LUA_TNIL) {
        if (lua_type(l, 3) != LUA_TSTRING) {
            return haveluaerror(l, badargument1, 2, "blitwizard.object:new",
            "string", lua_strtype(l, 3));
        }
        resource = lua_tostring(l, 3);
    }

    // create new object
    struct blitwizardobject* o = malloc(sizeof(*o));
    if (!o) {
        luacfuncs_object_clearRegistryTable(l, o);
        return haveluaerror(l, "Failed to allocate new object");
    }
    memset(o, 0, sizeof(*o));
    o->is3d = is3d;

    // add us to the object list:
    o->next = objects;
    if (objects) {
        objects->prev = o;
    }
    objects = o;

    // if resource is present, start loading it:
    luafuncs_objectgraphics_load(o, resource);

    // push idref to object onto stack as return value:
    luacfuncs_pushbobjidref(l, o);
    return 1;
}

void luacfuncs_object_clearRegistryTable(lua_State* l,
struct blitwizardobject* o) {
    char s[128];
    // compose object registry entry string
    snprintf(s, sizeof(s), "bobj_userdata_table_%p", o);

    // clear entry:
    lua_pushstring(l, s);
    lua_pushnil(l);
    lua_settable(l, LUA_REGISTRYINDEX);
}

void luacfuncs_object_obtainRegistryTable(lua_State* l,
struct blitwizardobject* o) {
    // obtain the hidden table that makes the blitwizard
    // object userdata behave like a table.

    char s[128];
    // compose object registry entry string
    snprintf(s, sizeof(s), "bobj_userdata_table_%p", o);

    // obtain registry entry:
    lua_pushstring(l, s);
    lua_gettable(l, LUA_REGISTRYINDEX);

    // if table is not present yet, create it:
    if (lua_type(l, -1) != LUA_TTABLE) {
        lua_pop(l, 1);  // pop whatever value this is

        // create a proper table:
        lua_pushstring(l, s);
        lua_newtable(l);
        lua_settable(l, LUA_REGISTRYINDEX);

        // obtain it again:
        lua_pushstring(l, s);
        lua_gettable(l, LUA_REGISTRYINDEX);
    }

    // resize stack:
    luaL_checkstack(l, 5, "insufficient stack to obtain object registry table");

    // the registry table's __index should go to
    // blitwizard.object:
    if (!lua_getmetatable(l, -1)) {
        // we need to create the meta table first:
        lua_newtable(l);
        lua_setmetatable(l, -2);

        // obtain it again:
        lua_getmetatable(l, -1);
    }
    lua_pushstring(l, "__index");
    lua_getglobal(l, "blitwizard");
    if (lua_type(l, -1) == LUA_TTABLE) {
        // extract blitwizard.object:
        lua_pushstring(l, "object");
        lua_gettable(l, -2);
        lua_insert(l, -2);
        lua_pop(l, 1);  // pop blitwizard table

        if (lua_type(l, -1) != LUA_TTABLE) {
            // error: blitwizard.object isn't a table as it should be
            lua_pop(l, 3); // blitwizard.object, "__index", metatable
        } else {
            lua_rawset(l, -3); // removes blitwizard.object, "__index"
            lua_setmetatable(l, -2); // setting remaining metatable
            // stack is now back empty, apart from new userdata!
        }
    } else {
        // error: blitwizard namespace is broken. nothing we could do
        lua_pop(l, 3);  // blitwizard, "__index", metatable
    }
}

// implicitely calls luacfuncs_onError() when an error happened
int luacfuncs_object_callEvent(lua_State* l,
struct blitwizardobject* o, const char* eventName,
int args) {
    char funcName[64];
    snprintf(funcName, sizeof(funcName),
    "blitwizard.object event function \"%s\"", eventName);

    // get object table:
    luacfuncs_object_obtainRegistryTable(l, o);

    // get event function
    lua_pushstring(l, eventName);
    lua_gettable(l, -2);

    // get rid of the object table again:
    lua_insert(l, -2);  // push function below table
    lua_pop(l, 1);  // remove table

    // push self as first argument:
    luacfuncs_pushbobjidref(l, o);
    if (args > 0) {
        lua_insert(l, -(args+1));
    }

    // move function in front of all args:
    if (args > 0) {
        lua_insert(l, -(args+2));
    }

    // push error handling function onto stack:
    lua_pushcfunction(l, internaltracebackfunc());

    // move error handling function in front of function + args
    lua_insert(l, -(args+3));

    int ret = lua_pcall(l, args+1, 0, -(args+3));

    int errorHappened = 0;
    // process errors:
    if (ret != 0) {
        const char* e = lua_tostring(l, -1);
        luacfuncs_onError(funcName, e);
    }

    // pop error handling function from stack again:
    lua_pop(l, 1);

    return (errorHappened == 0);
}

void luacfuncs_object_setEvent(lua_State* l,
struct blitwizardobject* o, const char* eventName) {
    // set the event function on top of the stack
    // to the blitwizard object

    // obtain registry table for object:
    luacfuncs_object_obtainRegistryTable(l, o);
    lua_insert(l, -2);  // move it below event func on stack

    // set function to table:
    char func[512];
    snprintf(func, sizeof(func), "eventfunc_%s", eventName);
    lua_pushstring(l, func);
    lua_insert(l, -2);  // move string below event func
    lua_settable(l, -3); 
}

/// Destroy the given object explicitely, to make it instantly disappear
// from the game world.
//
// If you still have references to this object, they will no longer
// work.
// @function destroy
int luafuncs_object_destroy(lua_State* l) {
    // delete the given object
    struct blitwizardobject* o = toblitwizardobject(l, 1, 1, "blitwiz.object.delete");
    if (o->deleted) {
        lua_pushstring(l, "Object was deleted");
        return lua_error(l);
    }

    // mark it deleted, and move it over to deletedobjects:
    o->deleted = 1;
    if (o->prev) {
      o->prev->next = o->next;
    } else {
      objects = o->next;
    }
    if (o->next) {
      o->next->prev = o->prev;
    }
    o->next = deletedobjects;
    deletedobjects->prev = o;
    deletedobjects = o;
    o->prev = NULL;

    // destroy the drawing and physics things attached to it:
    cleanupobject(o);
    return 0;
}

/// Get the current position of the object.
// Returns two coordinates for a 2d object, and three coordinates
// for a 3d object.
// @function getPosition
// @treturn number x coordinate
// @treturn number y coordinate
// @treturn number (if 3d object) z coordinate
int luafuncs_object_getPosition(lua_State* l) {
    struct blitwizardobject* obj = toblitwizardobject(l, 1, 0,
    "blitwizard.object:getPosition");
    if (obj->deleted) {
        return haveluaerror(l, "Object was deleted");
    }
    double x,y,z;
    objectphysics_getPosition(obj, &x, &y, &z);
    lua_pushnumber(l, x);
    lua_pushnumber(l, y);
    if (obj->is3d) {
        lua_pushnumber(l, z);
        return 3;
    }
    return 2;
}

/// Set the object to a new position.
// @function setPosition
// @tparam number pos_x x coordinate
// @tparam number pos_y y coordinate
// @tparam number pos_z (only for 3d objects) z coordinate
int luafuncs_object_setPosition(lua_State* l) {
    return 0;
}

/// Set the z-index of the object (only for 2d objects).
// An object with a higher z index will be drawn above
// others with a lower z index. If two objects have the same
// z index, the newer object will be drawn on top.
//
// The z index will be internally set to an integer,
// so use numbers like 1, 2, 3, 99, ...
// @function setZIndex
// @tparam number z_index New z index
int luafuncs_object_setZIndex(lua_State* l) {
    return 0;
}


