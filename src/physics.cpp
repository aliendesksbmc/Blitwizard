
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

#include "physics.h"
#include "Box2D/Box2D.h"
#include <stdint.h>

extern "C" {

struct physicsworld {
	b2World* w;
};

struct physicsworld* physics_CreateWorld() {
	struct physicsworld* world = (struct physicsworld*)malloc(sizeof(*world));
	if (!world) {
		return NULL;
	}
	memset(world, 0, sizeof(*world));
	b2Vec2 gravity(0.0f, 0.0f);
	world->w = new b2World(gravity);
	world->w->SetAllowSleeping(true);
	return world;
}

void physics_DestroyWorld(struct physicsworld* world) {
	delete world->w;
	free(world);
}

int physics_GetStepSize(struct physicsworld* world) {
	return (1000/50);
}

void physics_Step(struct physicsworld* world) {
	world->w->Step(1.0 / 50, 8, 3);
}

} //extern "C"

