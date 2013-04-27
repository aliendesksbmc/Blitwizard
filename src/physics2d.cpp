
/* blitwizard 2d engine - source code file

  Copyright (C) 2011-2012 Jonas Thiem

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

/* TODO:
    - Currently, specifying offset/rotation before setting up the shape will
     result in undefined behaviour. Change this?
    - Actually implement offsets, not just have them sit idly in the shape
     struct - DONE?
    - struct init stuff
    - Look at the edge and poly functions again (linked lists done correctly?)
    - Properly refactor the object creation function
    - Look for memory leaks (lol)
    - Add guard clauses for validating parameters (or don't)
*/

#if defined(USE_PHYSICS2D) || defined(USE_PHYSICS3D)

#ifdef USE_PHYSICS2D
#include <Box2D.h>
#endif

#include <stdint.h>
#define _USE_MATH_DEFINES
#include <cmath>
#include <stdio.h>

#define EPSILON 0.0001

#include "physics.h"
#include "mathhelpers.h"
#include "logging.h"

#ifndef NDEBUG
#define BW_E_NO3DYET "Error: 3D is not yet implemented."
#endif


extern "C" {

// TODO (cleanup, rename, ...):
/*
    Unsorted 2D-specific stuff
*/
class mycontactlistener;
class mycallback;
static int insidecollisioncallback = 0;


/*
    Enums, ...
*/
#ifdef USE_PHYSICS2D
enum shape_types_2d { BW_S2D_RECT=0, BW_S2D_POLY, BW_S2D_CIRCLE, BW_S2D_EDGE,
 BW_S2D_UNINITIALISED };
#endif


/*
    Purely internal structs
*/
#ifdef USE_PHYSICS2D
struct physicsobject2d;
struct physicsworld2d;
struct physicsobjectshape2d;
#endif
#ifdef USE_PHYSICS3D
struct physicsobject3d;
struct physicsworld3d;
struct physicsobjectshape3d;
#endif

/*
    Purely internal functions
*/
void _physics_Destroy2dShape(struct physicsobjectshape2d* shape);
int _physics_Check2dEdgeLoop(struct edge* edge, struct edge* target);
void _physics_Add2dShapeEdgeList_Do(struct physicsobjectshape* shape, double x1, double y1, double x2, double y2);
static struct physicsobject2d* _physics_Create2dObj(struct physicsworld2d* world, struct physicsobject* object, void* userdata, int movable);
void _physics_Create2dObjectEdges_End(struct edge* edges, struct physicsobject2d* object);
void _physics_Create2dObjectPoly_End(struct polygonpoint* polygonpoints, struct physicsobject2d* object);
static void _physics_Destroy2dObjectDo(struct physicsobject2d* obj);

/*
    Structs
*/

/*
    General (2D/3D) structs
*/
struct physicsobject {
    union dimension_specific_object {
#ifdef USE_PHYSICS2D
        struct physicsobject2d* ect2d;
#endif
#ifdef USE_PHYSICS3D
        struct physicsobject3d* ect3d;
#endif
    } obj;
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    int is3d;
#endif
    struct physicsworld* pworld;
};

struct physicsworld {
    union dimension_specific_world {
#ifdef USE_PHYSICS2D
        struct physicsworld2d* ld2d;
#endif
#ifdef USE_PHYSICS3D
        struct physicsworld3d* ld3d;
#endif
    } wor;
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    int is3d;
#endif
    void* callbackuserdata;
    int (*callback)(void* userdata, struct physicsobject* a, struct physicsobject* b, double x, double y, double normalx, double normaly, double force);
};

struct physicsobjectshape {
    union dimension_specific_shape {
#ifdef USE_PHYSICS2D
        struct physicsobjectshape2d* pe2d;
#endif
#ifdef USE_PHYSICS3D
        struct physicsobjectshape3d* pe3d;
#endif
    } sha;
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    int is3d;
#endif
};


/*
    2D-specific structs
*/
struct physicsworld2d {
    mycontactlistener* listener;
    b2World* w;
    double gravityx,gravityy;
    void* callbackuserdata; // FIXME: remove once appropriate
    int (*callback)(void* userdata, struct physicsobject2d* a, struct physicsobject2d* b, double x, double y, double normalx, double normaly, double force); // FIXME: remove once appropriate
};

struct physicsobject2d {
    int movable;
    b2World* world;
    b2Body* body;
    int gravityset;
    double gravityx,gravityy;
    void* userdata;
    struct physicsworld2d* pworld; // FIXME: remove once appropriate
    int deleted; // 1: deleted inside collision callback, 0: everything normal
};

struct physicsobjectshape2d {
    union specific_type_of_shape {
        struct rectangle2d* rectangle;
        struct polygonpoint* polygonpoints;
        b2CircleShape* circle;
        struct edge* edges;
    } b2;
    enum shape_types_2d type;
    double xoffset;
    double yoffset;
    double rotation;
};

struct deletedphysicsobject2d {
    struct physicsobject2d* obj;
    struct deletedphysicsobject2d* next;
};

struct deletedphysicsobject2d* deletedlist = NULL;

struct bodyuserdata {
    void* userdata;
    struct physicsobject* pobj;
};

struct rectangle2d {
    double width, height;
    b2PolygonShape* b2polygon;
};

struct edge {
    int inaloop;
    int processed;
    int adjacentcount;
    double x1,y1,x2,y2;
    struct edge* next;
    struct edge* adjacent1, *adjacent2;
};

struct polygonpoint {
  double x,y;
  struct polygonpoint* next;
};

struct physicsobject2dedgecontext {
    struct physicsobject2d* obj;
    double friction;
    struct edge* edgelist;
};

class mycontactlistener : public b2ContactListener {
public:
    mycontactlistener();
    ~mycontactlistener();
private:
    void PreSolve(b2Contact *contact, const b2Manifold *oldManifold);
};

mycontactlistener::mycontactlistener() {return;}
mycontactlistener::~mycontactlistener() {return;}

void mycontactlistener::PreSolve(b2Contact *contact, const b2Manifold *oldManifold) {
    struct physicsobject* obj1 = ((struct bodyuserdata*)contact->GetFixtureA()->GetBody()->GetUserData())->pobj;
    struct physicsobject* obj2 = ((struct bodyuserdata*)contact->GetFixtureB()->GetBody()->GetUserData())->pobj;
    if (obj1->obj.ect2d->deleted || obj2->obj.ect2d->deleted) {
        // one of the objects should be deleted already, ignore collision
        contact->SetEnabled(false);
        return;
    }

    // get collision point (this is never really accurate, but mostly sufficient)
    int n = contact->GetManifold()->pointCount;
    b2WorldManifold wmanifold;
    contact->GetWorldManifold(&wmanifold);
    float collidex = wmanifold.points[0].x;
    float collidey = wmanifold.points[0].y;
    float divisor = 1;
    int i = 1;
    while (i < n) {
        collidex += wmanifold.points[i].x;
        collidey += wmanifold.points[i].y;
        divisor += 1;
        i++;
    }
    collidex /= divisor;
    collidey /= divisor;

    // get collision normal ("push out" direction)
    float normalx = wmanifold.normal.x;
    float normaly = wmanifold.normal.y;

    // impact force:
    float impact = contact->GetManifold()->points[0].normalImpulse; //oldManifold->points[0].normalImpulse; //impulse->normalImpulses[0];

    // find our current world
    struct physicsworld* w = obj1->pworld;

    // return the information through the callback
    if (w->callback) {
        if (!w->callback(w->callbackuserdata, obj1, obj2, collidex, collidey, normalx, normaly, impact)) {
            // contact should be disabled:
            contact->SetEnabled(false);
        }else{
            // contact should remain enabled:
            contact->SetEnabled(true);
        }
    }
}

/*
    Struct helper functions
*/
inline int _physics_ObjIs3D(struct physicsobject* object) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    return object->is3d;
#elif defined(USE_PHYSICS2D)
    return 0;
#elif defined(USE_PHYSICS3D)
    return 1;
#endif
}

inline void _physics_SetObjIs3D(struct physicsobject* object, int is3d) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    object->is3d = is3d;
#endif
}

inline int _physics_ObjIsInit(struct physicsobject* object) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (object->is3d == -1)
#elif defined(USE_PHYSICS2D)
    if (object->obj.ect2d == NULL)
#elif defined(USE_PHYSICS3D)
    if (object->obj.ect3d == NULL)
#endif
        return 0;
    else
        return 1;
}

inline void _physics_ResetObj(struct physicsobject* object) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    object->is3d = -1;
#elif defined(USE_PHYSICS2D)
    object->obj.ect2d = NULL;
#elif defined(USE_PHYSICS3D)
    object->obj.ect3d = NULL;
#endif
}

inline int _physics_WorldIs3D(struct physicsworld* world) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    return world->is3d;
#elif defined(USE_PHYSICS2D)
    return 0;
#elif defined(USE_PHYSICS3D)
    return 1;
#endif
}

inline void _physics_SetWorldIs3D(struct physicsworld* world, int is3d) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    world->is3d = is3d;
#endif
}

// Isn't really needed since worlds are always initialised from the very beginning, anyway...
inline int _physics_WorldIsInit(struct physicsworld* world) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (world->is3d == -1)
#elif defined(USE_PHYSICS2D)
    if (world->wor.ld2d == NULL)
#elif defined(USE_PHYSICS3D)
    if (world->wor.ld3d == NULL)
#endif
        return 0;
    else
        return 1;
}

inline void _physics_ResetWorld(struct physicsworld* world) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    world->is3d = -1;
#elif defined(USE_PHYSICS2D)
    world->wor.ld2d = NULL;
#elif defined(USE_PHYSICS3D)
    world->wor.ld3d = NULL;
#endif
}

inline int _physics_ShapeIs3D(struct physicsobjectshape* shape) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    return shape->is3d;
#elif defined(USE_PHYSICS2D)
    return 0;
#elif defined(USE_PHYSICS3D)
    return 1;
#endif
}

inline void _physics_SetShapeIs3D(struct physicsobjectshape* shape, int is3d) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    shape->is3d = is3d;
#endif
}

inline int _physics_ShapeIsInit(struct physicsobjectshape* shape) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (shape->is3d == -1)
#elif defined(USE_PHYSICS2D)
    if (shape->sha.pe2d == NULL)
#elif defined(USE_PHYSICS3D)
    if (shape->sha.pe3d == NULL)
#endif
        return 0;
    else
        return 1;
}

// -1: None; 0: 2D; 1: 3D
inline int _physics_ShapeType(struct physicsobjectshape* shape) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    return shape->is3d;
#elif defined(USE_PHYSICS2D)
    if (shape->sha.pe2d != NULL) {
        return 0;
    }else{
        return -1;
    }
#elif defined(USE_PHYSICS3D)
    if (shape->sha.pe3d != NULL) {
        return 1;
    }else{
        return -1;
    }
#endif
}

inline void _physics_ResetShape(struct physicsobjectshape* shape) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    shape->is3d = -1;
#elif defined(USE_PHYSICS2D)
    shape->sha.pe2d = NULL;
#elif defined(USE_PHYSICS3D)
    shape->sha.pe3d = NULL;
#endif
}


/*
    Functions
*/
struct physicsworld* physics_CreateWorld(int use3dphysics) {
    struct physicsworld* world = (struct physicsworld*)malloc(sizeof(*world));
    if (!world) {
        return NULL;
    }
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (not use3dphysics) {
#endif
#ifdef USE_PHYSICS2D
    struct physicsworld2d* world2d = (struct physicsworld2d*)malloc(sizeof(*world2d));
    if (!world2d) {
        return NULL;
    }
    memset(world2d, 0, sizeof(*world2d));
    b2Vec2 gravity(0.0f, 0.0f);
    world2d->w = new b2World(gravity);
    world2d->w->SetAllowSleeping(true);
    world2d->gravityx = 0;
    world2d->gravityy = 10;
    world2d->listener = new mycontactlistener();
    world2d->w->SetContactListener(world2d->listener);
    world->wor.ld2d = world2d;
    _physics_SetWorldIs3D(world, 1);
    return world;
#else
    printerror("Error: Trying to create 2D physics world, but USE_PHYSICS2D is disabled.");
    return NULL;
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
    return NULL;
#else
    printerror("Error: Trying to create 3D physics world, but USE_PHYSICS3D is disabled.");
    return NULL;
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
}

void physics_DestroyWorld(struct physicsworld* world) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (not world->is3d) {
#endif
#ifdef USE_PHYSICS2D
    delete world->wor.ld2d->listener;
    delete world->wor.ld2d->w;
    free(world->wor.ld2d);
    free(world);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
}

void physics_Step(struct physicsworld* world) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (world->is3d) {
#endif
#ifdef USE_PHYSICS2D
    struct physicsworld2d* world2d = world->wor.ld2d;
    // Do a collision step
    insidecollisioncallback = 1; // remember we are inside a step
    int i = 0;
    while (i < 2) {
        double forcefactor = (1.0/(1000.0f/physics_GetStepSize(world)))*2;
        b2Body* b = world2d->w->GetBodyList();
        while (b) {
            // obtain physics object struct from body
            struct physicsobject* obj = ((struct bodyuserdata*)b->GetUserData())->pobj;
            struct physicsobject2d* obj2d = obj->obj.ect2d;
            if (obj) {
                if (obj2d->gravityset) {
                    // custom gravity which we want to apply
                    b->ApplyLinearImpulse(b2Vec2(obj2d->gravityx * forcefactor, obj2d->gravityy * forcefactor), b2Vec2(b->GetPosition().x, b->GetPosition().y));
                }else{
                    // no custom gravity -> apply world gravity
                    b->ApplyLinearImpulse(b2Vec2(world2d->gravityx * forcefactor, world2d->gravityy * forcefactor), b2Vec2(b->GetPosition().x, b->GetPosition().y));
                }
            }
            b = b->GetNext();
        }
#if defined(ANDROID) || defined(__ANDROID__)
        // less accurate on Android
        int it1 = 4;
        int it2 = 2;
#else
        // more accurate on desktop
        int it1 = 7;
        int it2 = 4;
#endif
        world2d->w->Step(1.0 /(1000.0f/physics_GetStepSize(world)), it1, it2);
        i++;
    }
    insidecollisioncallback = 0; // we are no longer inside a step

    // actually delete objects marked for deletion during the step:
    while (deletedlist) {
        // delete first object in the queue
        _physics_Destroy2dObjectDo(deletedlist->obj);

        // update list pointers (-> remove object from queue)
        struct deletedphysicsobject2d* pobj = deletedlist;
        deletedlist = deletedlist->next;

        // free removed object
        free(pobj);
    }
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
}

int physics_GetStepSize(struct physicsworld* world) {
    // TODO: 3D?
#if defined(ANDROID) || defined(__ANDROID__)
    // less accurate physics on Android (40 FPS)
    return (1000/40);
#else
    // more accurate physics on desktop (60 FPS)
    return (1000/60);
#endif
}


#ifdef USE_PHYSICS2D
void physics_Set2dCollisionCallback(struct physicsworld* world, int (*callback)(void* userdata, struct physicsobject* a, struct physicsobject* b, double x, double y, double normalx, double normaly, double force), void* userdata) {
    world->callback = callback;
    world->callbackuserdata = userdata;
}
#endif

#ifdef USE_PHYSICS3D
void physics_Set3dCollisionCallback(struct physicsworld* world, int (*callback)(void* userdata, struct physicsobject* a, struct physicsobject* b, double x, double y, double z, double normalx, double normaly, double normalz, double force), void* userdata) {
    printerror(BW_E_NO3DYET);
}
#endif


/* Shapes start here I guess */
struct physicsobjectshape* physics_CreateEmptyShapes(int count) {
    struct physicsobjectshape* shapes = (struct physicsobjectshape*)malloc((sizeof(*shapes))*count);
    // this was in earlier code so I'm doing it here as well, though tbh I don't understand the point
    if (!shapes) {
        return NULL;
    }
    int i = 0;
    while (i < count) {
        _physics_ResetShape(&(shapes[i]));
        ++i;
    }
    return shapes;
}

#ifdef USE_PHYSICS2D
void _physics_Destroy2dShape(struct physicsobjectshape2d* shape) {
    switch ((int)(shape->type)) {
        case BW_S2D_RECT:
            free(shape->b2.rectangle->b2polygon);
            free(shape->b2.rectangle);
        break;
        case BW_S2D_POLY:
            struct polygonpoint* p,* p2;
            p = shape->b2.polygonpoints;
            p2 = NULL;
            while (p != NULL) {
                p2 = p->next;
                free(p);
            }
            free(p2);
        break;
        case BW_S2D_CIRCLE:
            free(shape->b2.circle);
        break;
        case BW_S2D_EDGE:
            struct edge* e,* e2;
            e = shape->b2.edges;
            e2 = NULL;
            while (e != NULL) {
                e2 = e->next;
                free(e);
            }
            free(e2);
        break;
        default:
            printerror("Error: Unknown 2D shape type.");
    }
    free(shape);
}
#endif

void physics_DestroyShapes(struct physicsobjectshape* shapes, int count) {
    int i = 0;
    while (i < count) {
        switch (_physics_ShapeType(&(shapes[i]))) {
            case -1:
                // not initialised -> do nothing special
            break;
#ifdef USE_PHYSICS2D
            case 0:
                // 2D
                _physics_Destroy2dShape(shapes[i].sha.pe2d);
            break;
#endif
#ifdef USE_PHYSICS3D
            case 1:
                // 3D
                printerror(BW_E_NO3DYET);
            break;
#endif
        }
        ++i;
    }
    // do this no matter what
    free(shapes);
}

size_t physics_GetShapeSize(void) {
    return sizeof(struct physicsobjectshape*);
}


#ifdef USE_PHYSICS2D
struct physicsobjectshape2d* _physics_CreateEmpty2dShape() {
    struct physicsobjectshape2d* shape2d = (struct physicsobjectshape2d*)\
     malloc(sizeof(*shape2d));
    shape2d->type = BW_S2D_UNINITIALISED;
    shape2d->xoffset = 0;
    shape2d->yoffset = 0;
    shape2d->rotation = 0;
    return shape2d;
}
#endif

#ifdef USE_PHYSICS2D
void physics_Set2dShapeRectangle(struct physicsobjectshape* shape, double width, double height) {
/*
 it is of critical importance for all of this shit (i.e. functions below as well) that the object is absolutely uninitialised
   or had its former shape data deleted when this function (or functions below) is called. otherwise:
   XXX MEMORY LEAKS XXX
  checks needed?
*/
    /* Reasoning for storing w,h and a b2PolygonShape as well:
     Having the shape "cached" should make rapid creation of objects from this
     shape faster, whereas w,h are needed for offset/rotation stuff (not likely
     to be executed a lot).
    */
    
#ifndef NDEBUG
    if (shape == NULL) {
        printerror("Trying to apply physics_Set2dShapeRectangle() to NULL "\
         "shape.");
        return;
    }
    switch (_physics_ShapeType(shape)) {
        case 1:
            if (shape->sha.pe2d->type != BW_S2D_UNINITIALISED) {
                printerror("Trying to apply physics_Set2dShapeRectangle() to "
                 "already initialised shape.");
                return;
            }
        break;
        case 2:
            printerror("Trying to apply physics_Set2dShapeRectangle() to 3D "\
             "shape.");
            return;
        break;
    }
#endif
    
    struct rectangle2d* rectangle = (struct rectangle2d*)malloc(
     sizeof(*rectangle));
    if (!rectangle)
        return;
    
    /* Get offset/rotation from shape only if present (that is, if shape is a
     2D shape, though does not necessarily include actual b2 shapes, just the
     physicsobjectshape2d struct itself)
    */
    b2Vec2 center(0, 0);
    double rotation = 0;
    if (_physics_ShapeType(shape) == 1) {
        center.x = shape->sha.pe2d->xoffset;
        center.y = shape->sha.pe2d->yoffset;
        rotation = shape->sha.pe2d->rotation;
    }else{
        shape->sha.pe2d = _physics_CreateEmpty2dShape();
    }
    
    rectangle->width = width;
    rectangle->height = height;
    struct b2PolygonShape* box = (struct b2PolygonShape*)malloc(sizeof(*box));
    box->SetAsBox((width/2) - box->m_radius*2, (height/2) - box->m_radius*2,
     center, rotation);
    rectangle->b2polygon = box;;
    shape->sha.pe2d->b2.rectangle = rectangle;
    shape->sha.pe2d->type = BW_S2D_RECT;
    
#ifdef USE_PHYSICS3D
    shape->is3d = 0;
#endif
}
#endif

#ifdef USE_PHYSICS2D
#define OVALVERTICES 8
void physics_Set2dShapeOval(struct physicsobjectshape* shape, double width, double height) {
    /* FIXME: Order might the "wrong way round", then again this shouldn't really matter. */
    if (fabs(width - height) < EPSILON) {
        physics_Set2dShapeCircle(shape, width);
        return;
    }

    //construct oval shape - by manually calculating the vertices
    struct polygonpoint* vertices = (struct polygonpoint*)malloc(sizeof(*vertices)*OVALVERTICES);
    int i = 0;
    double angle = 0;
    
    // TODO: do it like with the rectangle thing maybe
    /*b2Vec2 center(0, 0);
    double rotation = 0;
    if (_physics_ShapeType(shape) == 1) {
        center.x = shape->sha.pe2d->xoffset;
        center.y = shape->sha.pe2d->yoffset;
        rotation = shape->sha.pe2d->rotation;
    }else{*/
    shape->sha.pe2d = _physics_CreateEmpty2dShape();
    //}
    
    //go around with the angle in one full circle:
    while (angle < 2*M_PI && i < OVALVERTICES) {
        //calculate and set vertex point
        double x,y;
        ovalpoint(angle, width, height, &x, &y);
        vertices[i].x = x;
        vertices[i].y = -y;
        vertices[i].next = &vertices[i+i];
        
        //advance to next position
        angle -= (2*M_PI)/((double)OVALVERTICES);
        i++;
    }
    vertices[i-1].next = NULL;
    
    shape->sha.pe2d->b2.polygonpoints = vertices;
    shape->sha.pe2d->type = BW_S2D_POLY;
#ifdef USE_PHYSICS3D
    shape->is3d = 0;
#endif
}
#endif

#ifdef USE_PHYSICS2D
void physics_Set2dShapeCircle(struct physicsobjectshape* shape, double diameter) {
    b2CircleShape* circle = (b2CircleShape*)malloc(sizeof(*circle));
    double radius = diameter/2;
    circle->m_radius = radius - 0.01;
    
    // TODO: do it like with the rectangle thing maybe
    /*b2Vec2 center(0, 0);
    double rotation = 0;
    if (_physics_ShapeType(shape) == 1) {
        center.x = shape->sha.pe2d->xoffset;
        center.y = shape->sha.pe2d->yoffset;
        rotation = shape->sha.pe2d->rotation;
    }else{*/
    shape->sha.pe2d = _physics_CreateEmpty2dShape();
    //}
    
    shape->sha.pe2d->b2.circle = circle;
    shape->sha.pe2d->type = BW_S2D_CIRCLE;
#ifdef USE_PHYSICS3D
    shape->is3d = 0;
#endif
}
#endif

#ifdef USE_PHYSICS2D
void physics_Add2dShapePolygonPoint(struct physicsobjectshape* shape, double xoffset, double yoffset) {
    // TODO: do it like with the rectangle thing
    /*b2Vec2 center(0, 0);
    double rotation = 0;*/
    if (_physics_ShapeType(shape) == 1) {
        /*center.x = shape->sha.pe2d->xoffset;
        center.y = shape->sha.pe2d->yoffset;
        rotation = shape->sha.pe2d->rotation;*/
    }else{
    shape->sha.pe2d = _physics_CreateEmpty2dShape();
    }
    
    if (not shape->sha.pe2d->type == BW_S2D_POLY) {
        shape->sha.pe2d->b2.polygonpoints = NULL;
    }
    struct polygonpoint* p = shape->sha.pe2d->b2.polygonpoints;
    
    struct polygonpoint* new_point = (struct polygonpoint*)malloc(sizeof(*new_point));
    new_point->x = xoffset;
    new_point->y = yoffset;
    
    if (p != NULL) {
      new_point->next = p;
    }
    p = new_point;
    
    shape->sha.pe2d->type = BW_S2D_POLY;
#ifdef USE_PHYSICS3D
    shape->is3d = 0;
#endif
}
#endif

#ifdef USE_PHYSICS2D
int _physics_Check2dEdgeLoop(struct edge* edge, struct edge* target) {
    struct edge* e = edge;
    struct edge* eprev = NULL;
    while (e) {
        if (e == target) {return 1;}
        struct edge* nextprev = e;
        if (e->adjacent1 && e->adjacent1 != eprev) {
            e = e->adjacent1;
        }else{
            if (e->adjacent2 && e->adjacent2 != eprev) {
                e = e->adjacent2;
            }else{
                e = NULL;
            }
        }
        eprev = nextprev;
    }
    return 0;
}
#endif

#ifdef USE_PHYSICS2D
void _physics_Add2dShapeEdgeList_Do(struct physicsobjectshape* shape, double x1, double y1, double x2, double y2) {
    struct edge* newedge = (struct edge*)malloc(sizeof(*newedge));
    if (!newedge) {return;}
    memset(newedge, 0, sizeof(*newedge));
    newedge->x1 = x1;
    newedge->y1 = y1;
    newedge->x2 = x2;
    newedge->y2 = y2;
    newedge->adjacentcount = 1;
    
    //search for adjacent edges
    struct edge* e = shape->sha.pe2d->b2.edges;
    while (e) {
        if (!newedge->adjacent1) {
            if (fabs(e->x1 - newedge->x1) < EPSILON && fabs(e->y1 - newedge->y1) < EPSILON && e->adjacent1 == NULL) {
                if (_physics_Check2dEdgeLoop(e, newedge)) {
                    newedge->inaloop = 1;
                }else{
                    e->adjacentcount += newedge->adjacentcount;
                    newedge->adjacentcount = e->adjacentcount;
                }
                newedge->adjacent1 = e;
                e->adjacent1 = newedge;
                e = e->next;
                continue;
            }
            if (fabs(e->x2 - newedge->x1) < EPSILON && fabs(e->y2 - newedge->y1) < EPSILON && e->adjacent2 == NULL) {
                if (_physics_Check2dEdgeLoop(e, newedge)) {
                    newedge->inaloop = 1;
                }else{
                    e->adjacentcount += newedge->adjacentcount;
                    newedge->adjacentcount = e->adjacentcount;
                }
                newedge->adjacent1 = e;
                e->adjacent2 = newedge;
                e = e->next;
                continue;
            }
        }
        if (!newedge->adjacent2) {
            if (fabs(e->x1 - newedge->x2) < EPSILON && fabs(e->y1 - newedge->y2) < EPSILON && e->adjacent1 == NULL) {
                if (_physics_Check2dEdgeLoop(e, newedge)) {
                    newedge->inaloop = 1;
                }else{
                    e->adjacentcount += newedge->adjacentcount;
                    newedge->adjacentcount = e->adjacentcount;
                }
                newedge->adjacent2 = e;
                e->adjacent1 = newedge;
                e = e->next;
                continue;
            }
            if (fabs(e->x2 - newedge->x2) < EPSILON && fabs(e->y2 - newedge->y2) < EPSILON && e->adjacent2 == NULL) {
                if (_physics_Check2dEdgeLoop(e, newedge)) {
                    newedge->inaloop = 1;
                }else{
                    e->adjacentcount += newedge->adjacentcount;
                    newedge->adjacentcount = e->adjacentcount;
                }
                newedge->adjacent2 = e;
                e->adjacent2 = newedge;
                e = e->next;
                continue;
            }
        }
        e = e->next;
    }

    //add us to the unsorted linear list
    newedge->next = shape->sha.pe2d->b2.edges;
    shape->sha.pe2d->b2.edges = newedge;
}
#endif

#ifdef USE_PHYSICS2D
void physics_Add2dShapeEdgeList(struct physicsobjectshape* shape, double x1, double y1, double x2, double y2) {
    // TODO: do it like with the rectangle thing
    /*b2Vec2 center(0, 0);
    double rotation = 0;*/
    if (_physics_ShapeType(shape) == 1) {
        /*center.x = shape->sha.pe2d->xoffset;
        center.y = shape->sha.pe2d->yoffset;
        rotation = shape->sha.pe2d->rotation;*/
    }else{
    shape->sha.pe2d = _physics_CreateEmpty2dShape();
    }
    
    // FIXME FIXME FIXME this function might be complete bollocks, reconsider pls
    if (not shape->sha.pe2d->type == BW_S2D_EDGE) {
        shape->sha.pe2d->b2.edges = NULL;
    }
    
    _physics_Add2dShapeEdgeList_Do(shape, x1, y1, x2, y2);
    
    struct edge* e = shape->sha.pe2d->b2.edges;
    while (e != NULL and e->next != NULL) {
        e = e->next;
    }
    struct edge* new_edge = (struct edge*)malloc(sizeof(*new_edge));
    new_edge->x1 = x1;
    new_edge->y1 = y1;
    new_edge->x2 = x2;
    new_edge->y2 = y2;
    e->next = new_edge;
    
    shape->sha.pe2d->type = BW_S2D_EDGE;
#ifdef USE_PHYSICS3D
    shape->is3d = 0;
#endif
}
#endif

#ifdef USE_PHYSICS2D
inline void _physics_RotateThenOffset2dShapePoint(
 struct physicsobjectshape* shape,
 double xoffset, double yoffset, double rotation, double* x, double* y) {
    struct physicsobjectshape2d* s = shape->sha.pe2d;
    *x -= s->xoffset;
    *y -= s->yoffset;
    rotatevec(*x, *y, (s->rotation)-rotation, x, y);
    *x += xoffset;
    *y += yoffset;
}
#endif

#ifdef USE_PHYSICS2D
void physics_Set2dShapeOffsetRotation(struct physicsobjectshape* shape, double xoffset, double yoffset, double rotation) {
    /* XXX Note on this function: Since I expect the user will almost never call
     this thing twice, but will most definitely create several objects from
     the same shape struct at some point, all the offset application happens
     here already, NOT during object creation. */
    /* TODO: le fuck, given that our 2d shape struct contains rects and circles
     in their b2 form, the whole point of which was that it'd be more efficient
     - now it's not efficient at all, since we have to get their components,
     apply transforms, then create a new b2 shape and all
     UPDATE: applies to rectangles only, honestly fk rectangles */
    /* procedure:
        what already happened: 1.) rotate by rot_old 2.) apply x+xoffs_old, y+yoffs_old
        so what we have to do: 1.) subtract offs_old 2.) rotate by rot_old-rot_new 3.) add offs_new
        might be easier w/ matrices, but also more proc-time-consuming probably
    */
    struct physicsobjectshape2d* s = shape->sha.pe2d;
    // effective offsets and rotation
    /*double exoffset = xoffset-(s->xoffset);
    double eyoffset = yoffset-(s->yoffset);
    double erotation = rotation-(s->rotation);*/
    
    // stupid compiler
    b2PolygonShape* b2polygon = NULL;
    double new_x=0, new_y=0;
    b2Vec2 new_center;
    struct polygonpoint* p = NULL;
    struct edge* e = NULL;
    
    switch (s->type) {
        case BW_S2D_RECT:
            b2polygon = s->b2.rectangle->b2polygon;
            new_x = b2polygon->m_centroid.x;
            new_y = b2polygon->m_centroid.y;
            //new_center = b2polygon->m_centroid; // copy
            _physics_RotateThenOffset2dShapePoint(shape, xoffset, yoffset,
             rotation, &new_x, &new_y);
            new_center = b2Vec2(new_x, new_y);
            b2polygon->SetAsBox(s->b2.rectangle->width,
             s->b2.rectangle->height, new_center, rotation);
        break;
        case BW_S2D_POLY:
            p = s->b2.polygonpoints;
            while (p->next != NULL) {
                _physics_RotateThenOffset2dShapePoint(shape, xoffset, yoffset, 
                 rotation, &(p->x), &(p->y));
                p = p->next;
            }
        break;
        case BW_S2D_CIRCLE:
            /* Don't need rotation etc. here as it's a fricking circle and it
             doesn't have an offset of its own (i.e. seperate from this offset
             mechanism.
            */
            s->b2.circle->m_p.x = xoffset;
            s->b2.circle->m_p.y = yoffset;
        break;
        case BW_S2D_EDGE:
            e = s->b2.edges;
            while (e->next != NULL) {
                _physics_RotateThenOffset2dShapePoint(shape, xoffset, yoffset, 
                 rotation, &(e->x1), &(e->y1));
                _physics_RotateThenOffset2dShapePoint(shape, xoffset, yoffset, 
                 rotation, &(e->x2), &(e->y2));
                e = e->next;
            }
        break;
    
    }
    
    s->xoffset = xoffset;
    s->yoffset = yoffset;
    s->rotation = rotation;
}
#endif

#ifdef USE_PHYSICS2D
void physics_Get2dShapeOffsetRotation(struct physicsobjectshape* shape, double* xoffset, double* yoffset, double* rotation) {
    struct physicsobjectshape2d* s = shape->sha.pe2d;
    *xoffset = s->xoffset;
    *yoffset = s->yoffset;
    *rotation = s->rotation;
}
#endif

// Everything about object creation starts here
#ifdef USE_PHYSICS2D
static struct physicsobject2d* _physics_Create2dObj(struct physicsworld2d* world, struct physicsobject* object, void* userdata, int movable) {
    struct physicsobject2d* obj2d = (struct physicsobject2d*)malloc(sizeof(*obj2d));
    if (!obj2d) {return NULL;}
    memset(obj2d, 0, sizeof(*obj2d));

    struct bodyuserdata* pdata = (struct bodyuserdata*)malloc(sizeof(*pdata));
    if (!pdata) {
        free(obj2d);
        return NULL;
    }
    memset(pdata, 0, sizeof(*pdata));
    pdata->userdata = userdata;
    pdata->pobj = object;

    b2BodyDef bodyDef;
    if (movable) {
        bodyDef.type = b2_dynamicBody;
    }
    obj2d->movable = movable;
    bodyDef.userData = (void*)pdata;
    obj2d->userdata = pdata;
    obj2d->body = world->w->CreateBody(&bodyDef);
    obj2d->body->SetFixedRotation(false);
    obj2d->world = world->w;
    obj2d->pworld = world;
    if (!obj2d->body) {
        free(obj2d);
        free(pdata);
        return NULL;
    }
    return obj2d;
}
#endif

#ifdef USE_PHYSICS2D
// TODO: Weaken coupling, i.e. no direct reference to physicsobject2d, instead take edges and return b2 chains
// (goal: common code for fixture etc. in outer function)
// problem: memory mgmt., variable number of returned edge shapes
void _physics_Create2dObjectEdges_End(struct edge* edges, struct physicsobject2d* object) {
    /* not sure if this is needed anymore, probably not
    if (!context->edgelist) {
        physics2d_DestroyObject(context->obj);
        free(context);
        return NULL;
    }*/

    struct edge* e = edges;
    while (e) {
        //skip edges we already processed
        if (e->processed) {
            e = e->next;
            continue;
        }

        //only process edges which are start of an adjacent chain, in a loop or lonely
        if (e->adjacent1 && e->adjacent2 && !e->inaloop) {
            e = e->next;
            continue;
        }

        int varraysize = e->adjacentcount+1;
        b2Vec2* varray = new b2Vec2[varraysize];
        b2ChainShape chain;
        e->processed = 1;

        //see into which direction we want to go
        struct edge* eprev = e;
        struct edge* e2;
        if (e->adjacent1) {
            varray[0] = b2Vec2(e->x2, e->y2);
            varray[1] = b2Vec2(e->x1, e->y1);
            e2 = e->adjacent1;
        }else{
            varray[0] = b2Vec2(e->x1, e->y1);
            varray[1] = b2Vec2(e->x2, e->y2);
            e2 = e->adjacent2;
        }

        //ok let's take a walk:
        int i = 2;
        while (e2) {
            if (e2->processed) {break;}
            e2->processed = 1;
            struct edge* enextprev = e2;

            //Check which vertex we want to add
            if (e2->adjacent1 == eprev) {
                varray[i] = b2Vec2(e2->x2, e2->y2);
            }else{
                varray[i] = b2Vec2(e2->x1, e2->y1);
            }

            //advance to next edge
            if (e2->adjacent1 && e2->adjacent1 != eprev) {
                e2 = e2->adjacent1;
            }else{
                if (e2->adjacent2 && e2->adjacent2 != eprev) {
                    e2 = e2->adjacent2;
                }else{
                    e2 = NULL;
                }
            }
            eprev = enextprev;
            i++;
        }

        //debug print values
        /*int u = 0;
        while (u < e->adjacentcount + 1 - (1 * e->inaloop)) {
            printf("Chain vertex: %f, %f\n", varray[u].x, varray[u].y);
            u++;
        }*/
    
        //construct an edge shape from this
        if (e->inaloop) {
            chain.CreateLoop(varray, e->adjacentcount);
        }else{
            chain.CreateChain(varray, e->adjacentcount+1);
        }

        //add it to our body
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &chain;
        fixtureDef.friction = 1; // TODO: ???
        fixtureDef.density = 1; // TODO: ???
        object->body->CreateFixture(&fixtureDef);

        delete[] varray;
    }
}
#endif

#ifdef USE_PHYSICS2D
// TODO: cf. function above
void _physics_Create2dObjectPoly_End(struct polygonpoint* polygonpoints, struct physicsobject2d* object) {
    struct polygonpoint* p = polygonpoints;
    // TODO: cache this instead?
    int i = 0;
    while (p != NULL) {
        ++i;
        p = p->next;
    }
    b2Vec2* varray = new b2Vec2[i];
    p = polygonpoints;
    i = 0;
    while (p != NULL) {
        varray[i].Set(p->x, p->y);
        ++i;
        p = p->next;
    }
    b2PolygonShape shape;
    shape.Set(varray, i);
    
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.friction = 1; // TODO: ???
    fixtureDef.density = 1; // TODO: ???
    object->body->CreateFixture(&fixtureDef);
    
    delete[] varray;
}
#endif

struct physicsobject* physics_CreateObject(struct physicsworld* world, void* userdata, int movable, struct physicsobjectshape* shapelist) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (!world->is3d) {
#endif
#ifdef USE_PHYSICS2D
    struct physicsobject* obj = (struct physicsobject*)malloc(sizeof(*obj));
    struct physicsobject2d* obj2d = _physics_Create2dObj(world->wor.ld2d, obj, userdata, movable);
    if (obj2d == NULL) {
        return NULL;
    }
    
    struct physicsobjectshape* s = shapelist;
    while (s != NULL) {
        if (_physics_ShapeType(s) != 0) {
            // TODO: error msg?
            break;
        }
        b2FixtureDef fixtureDef;
        switch ((int)(s->sha.pe2d->type)) {
            case BW_S2D_RECT:
                fixtureDef.shape = s->sha.pe2d->b2.rectangle->b2polygon;
                fixtureDef.friction = 1; // TODO: ???
                fixtureDef.density = 1;
                obj2d->body->SetFixedRotation(false);
                obj2d->body->CreateFixture(&fixtureDef);
            break;
            case BW_S2D_POLY:
                _physics_Create2dObjectPoly_End(s->sha.pe2d->b2.polygonpoints, obj2d);
            break;
            case BW_S2D_CIRCLE:
                fixtureDef.shape = s->sha.pe2d->b2.circle;
                fixtureDef.friction = 1; // TODO: ???
                fixtureDef.density = 1;
                obj2d->body->SetFixedRotation(false);
                obj2d->body->CreateFixture(&fixtureDef);
            break;
            case BW_S2D_EDGE:
                _physics_Create2dObjectEdges_End(s->sha.pe2d->b2.edges, obj2d);
            break;
        }
        s += 1;
    }
    obj->obj.ect2d = obj2d;
#ifdef USE_PHYSICS3D
    obj->is3d = 0;
#endif
    physics_SetMass(obj, 0);
    return obj;
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif

}


// Everything about object deletion starts here
static void _physics_Destroy2dObjectDo(struct physicsobject2d* obj) {
    if (obj->body) {
        obj->world->DestroyBody(obj->body);
    }
    free(obj);
}

void physics_DestroyObject(struct physicsobject* obj) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (not obj->is3d) {
#endif
#ifdef USE_PHYSICS2D
    if (!obj->obj.ect2d || obj->obj.ect2d->deleted == 1) {
        return;
    }
    if (!insidecollisioncallback) {
        _physics_Destroy2dObjectDo(obj->obj.ect2d);
    }else{
        obj->obj.ect2d->deleted = 1;
        struct deletedphysicsobject2d* dobject = (struct deletedphysicsobject2d*)malloc(sizeof(*dobject));
        if (!dobject) {
            return;
        }
        memset(dobject, 0, sizeof(*dobject));
        dobject->obj = obj->obj.ect2d;
        dobject->next = deletedlist;
        deletedlist = dobject;
    }
    
    // ?
    free(obj);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
}


void* physics_GetObjectUserdata(struct physicsobject* object) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (not object->is3d) {
#endif
#ifdef USE_PHYSICS2D
    return ((struct bodyuserdata*)object->obj.ect2d->body->GetUserData())->userdata;
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
    return NULL;
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
    return NULL;
}


// Everything about "various properties" starts here
void physics_SetMass(struct physicsobject* obj, double mass) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (not obj->is3d) {
#endif
#ifdef USE_PHYSICS2D
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    if (!obj2d->movable) {return;}
    if (!obj2d->body) {return;}
    if (mass > 0) {
        if (obj2d->body->GetType() == b2_staticBody) {
            obj2d->body->SetType(b2_dynamicBody);
        }
    }else{
        mass = 0;
        if (obj2d->body->GetType() == b2_dynamicBody) {
            obj2d->body->SetType(b2_staticBody);
        }
    }
    b2MassData mdata;
    obj2d->body->GetMassData(&mdata);
    mdata.mass = mass;
    obj2d->body->SetMassData(&mdata);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
}

double physics_GetMass(struct physicsobject* obj) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (not obj->is3d) {
#endif
#ifdef USE_PHYSICS2D
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    b2MassData mdata;
    obj2d->body->GetMassData(&mdata);
    return mdata.mass;
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
}

#ifdef USE_PHYSICS2D
void physics_Set2dMassCenterOffset(struct physicsobject* obj, double offsetx, double offsety) {
    // TODO: checks for is3d or not?
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    b2MassData mdata;
    obj2d->body->GetMassData(&mdata);
    mdata.center = b2Vec2(offsetx, offsety);
    obj2d->body->SetMassData(&mdata);
}
#endif

#ifdef USE_PHYSICS2D
void physics_Get2dMassCenterOffset(struct physicsobject* obj, double* offsetx, double* offsety) {
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    b2MassData mdata;
    obj2d->body->GetMassData(&mdata);
    *offsetx = mdata.center.x;
    *offsety = mdata.center.y;
}
#endif

#ifdef USE_PHYSICS2D
void physics_Set2dGravity(struct physicsobject* obj, double x, double y) {
    // TODO: again: check for is3d or not?
    if (!obj) {return;}
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    obj2d->gravityset = 1;
    obj2d->gravityx = x;
    obj2d->gravityy = y;
}
#endif

void physics_UnsetGravity(struct physicsobject* obj) {
    if (!obj) {return;}
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (not obj->is3d) {
#endif
#ifdef USE_PHYSICS2D
    obj->obj.ect2d->gravityset = 0;
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
}

#ifdef USE_PHYSICS2D
void physics_Set2dRotationRestriction(struct physicsobject* obj, int restricted) {
    // TODO: CHECK IS3D YES OR NO FFFFF
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    if (!obj2d->body) {return;}
    if (restricted) {
        obj2d->body->SetFixedRotation(true);
    }else{
        obj2d->body->SetFixedRotation(false);
    }
}
#endif

void physics_SetFriction(struct physicsobject* obj, double friction) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (not obj->is3d) {
#endif
#ifdef USE_PHYSICS2D
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    if (!obj2d->body) {return;}
    b2Fixture* f = obj2d->body->GetFixtureList();
    while (f) {
        f->SetFriction(friction);
        f = f->GetNext();
    }
    b2ContactEdge* e = obj2d->body->GetContactList();
    while (e) {
        e->contact->ResetFriction();
        e = e->next;
    }
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
}

void physics_SetAngularDamping(struct physicsobject* obj, double damping) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (not obj->is3d) {
#endif
#ifdef USE_PHYSICS2D
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    if (!obj2d->body) {return;}
    obj2d->body->SetAngularDamping(damping);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
}

void physics_SetLinearDamping(struct physicsobject* obj, double damping) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (not obj->is3d) {
#endif
#ifdef USE_PHYSICS2D
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    if (!obj2d->body) {return;}
    obj2d->body->SetLinearDamping(damping);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
}

void physics_SetRestitution(struct physicsobject* obj, double restitution) {
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    if (not obj->is3d) {
#endif
#ifdef USE_PHYSICS2D
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    if (restitution > 1) {restitution = 1;}
    if (restitution < 0) {restitution = 0;}
    if (!obj2d->body) {return;}
    b2Fixture* f = obj2d->body->GetFixtureList();
    while (f) {
        f->SetRestitution(restitution);
        f = f->GetNext();
    }
    b2ContactEdge* e = obj2d->body->GetContactList();
    while (e) {
        e->contact->SetRestitution(restitution);
        e = e->next;
    }
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }else{
#endif
#ifdef USE_PHYSICS3D
    printerror(BW_E_NO3DYET);
#endif
#if defined(USE_PHYSICS2D) && defined(USE_PHYSICS3D)
    }
#endif
}

#ifdef USE_PHYSICS2D
void physics_Get2dPosition(struct physicsobject* obj, double* x, double* y) {
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    b2Vec2 pos = obj2d->body->GetPosition();
    *x = pos.x;
    *y = pos.y;
}
#endif

#ifdef USE_PHYSICS2D
void physics_Get2dRotation(struct physicsobject* obj, double* angle) {
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    *angle = (obj2d->body->GetAngle() * 180)/M_PI;
}
#endif

#ifdef USE_PHYSICS2D
void physics_Warp2d(struct physicsobject* obj, double x, double y, double angle) {
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    obj2d->body->SetTransform(b2Vec2(x, y), angle * M_PI / 180);
}
#endif

#ifdef USE_PHYSICS2D
void physics_Apply2dImpulse(struct physicsobject* obj, double forcex, double forcey, double sourcex, double sourcey) {
    struct physicsobject2d* obj2d = obj->obj.ect2d;
    if (!obj2d->body || !obj2d->movable) {return;}
    obj2d->body->ApplyLinearImpulse(b2Vec2(forcex, forcey), b2Vec2(sourcex, sourcey));
}
#endif

#ifdef USE_PHYSICS2D
class mycallback : public b2RayCastCallback {
public:
    b2Body* closestcollidedbody;
    b2Vec2 closestcollidedposition;
    b2Vec2 closestcollidednormal;

    mycallback() {
        closestcollidedbody = NULL;
    }

    virtual float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float32 fraction) {
        closestcollidedbody = fixture->GetBody();
        closestcollidedposition = point;
        closestcollidednormal = normal;
        return fraction;
    }
};
#endif

#ifdef USE_PHYSICS2D
int physics_Ray2d(struct physicsworld* world, double startx, double starty, double targetx, double targety, double* hitpointx, double* hitpointy, struct physicsobject** objecthit, double* hitnormalx, double* hitnormaly) {
    struct physicsworld2d* world2d = world->wor.ld2d;    
    
    // create callback object which finds the closest impact
    mycallback callbackobj;
    
    // cast a ray and have our callback object check for closest impact
    world2d->w->RayCast(&callbackobj, b2Vec2(startx, starty), b2Vec2(targetx, targety));
    if (callbackobj.closestcollidedbody) {
        // we have a closest collided body, provide hitpoint information:
        *hitpointx = callbackobj.closestcollidedposition.x;
        *hitpointy = callbackobj.closestcollidedposition.y;
        *objecthit = ((struct bodyuserdata*)callbackobj.closestcollidedbody->GetUserData())->pobj;
        *hitnormalx = callbackobj.closestcollidednormal.x;
        *hitnormaly = callbackobj.closestcollidednormal.y;
        return 1;
    }
    // no collision found
    return 0;
}
#endif


} //extern "C"

#endif

