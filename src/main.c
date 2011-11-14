
/* blitwizard 2d engine - source code file

  Copyright (C) 2011 Jonas Thiem

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

extern int drawingallowed; //stored in luafuncs.c
#include "luastate.h"
#include "file.h"
#include "graphics.h"
#include "timefuncs.h"

#define TIMESTEP 16

void fatalscripterror() {

}

void printerror(const char* fmt, ...) {
	char printline[2048];
        va_list a;
        va_start(a, fmt);
        vsnprintf(printline, sizeof(printline)-1, fmt, a);
        printline[sizeof(printline)-1] = 0;
        va_end(a);
	fprintf(stderr,"%s",printline);
	fflush(stderr);
}

static void imgloaded(int success, const char* texture) {
	char* error;
	if (!luastate_PushFunctionArgumentToMainstate_String(texture)) {
		printerror("Error when pushing func args to blitwizi.callback.event.image\n");
		fatalscripterror();
		return;
	}
	if (!luastate_PushFunctionArgumentToMainstate_Bool(success)) {
                printerror("Error when pushing func args to blitwizi.callback.event.image\n");
                fatalscripterror();
                return;
        }
	if (!luastate_CallFunctionInMainstate("blitwiz.callback.event.image", 2, 1, 1, &error)) {
		printerror("Error when calling blitwiz.callback.event.image: %s\n", error);
		if (error) {free(error);}
		fatalscripterror();
		return;
	}
}

int wantquit = 0;
int main(int argc, char** argv) {
	//evaluate command line arguments:
	const char* script = "game.lua";
	int i = 1;
	int scriptargfound = 0;
	int option_changedir = 0;
	while (i < argc) {
		if (argv[i][0] == '-' || strcasecmp(argv[i],"/?") == 0) {
			if (strcasecmp(argv[i],"--help") == 0 || strcasecmp(argv[i], "-help") == 0
			|| strcasecmp(argv[i], "-?") == 0 || strcasecmp(argv[i],"/?") == 0
			|| strcasecmp(argv[i],"-h") == 0) {
				printf("blitwizard %s\n",VERSION);
				printf("Usage: blitwizard [options] [lua script]\n");
				printf("   -changedir: Change working directory to script dir\n");
				printf("   -help: Show this help text and quit\n");
				return 0;
			}
			if (strcasecmp(argv[i],"-changedir") == 0) {
				option_changedir = 1;
				i++;
				continue;
			}
			printerror("Error: Unknown option: %s\n",argv[i]);
			return -1;
		}else{
			if (!scriptargfound) {
				scriptargfound = 1;
				script = argv[i];
			}
		}
		i++;
	}

	//check the provided path:
	char outofmem[] = "Out of memory";
	char* error;
	char* filenamebuf = NULL;

	//check if a folder:
	if (file_IsDirectory(script)) {
		filenamebuf = file_AddComponentToPath(script, "game.lua");
		script = filenamebuf;
	}

	//check if we want to change directory to the provided path:
	if (option_changedir) {
		char* p = file_GetAbsoluteDirectoryPathFromFilePath(script);
		if (!p) {
			printerror("Error: NULL returned for absolute directory\n");
			return -1;
		}
		char* newfilenamebuf = file_GetFileNameFromFilePath(script);
		if (!newfilenamebuf) {
			free(p);
			printerror("Error: NULL returned for file name\n");
			return -1;
		}
		if (filenamebuf) {free(filenamebuf);}
		filenamebuf = newfilenamebuf;
		if (!file_Cwd(p)) {
			free(filenamebuf);
			printerror("Error: Cannot cd to \"%s\"\n",p);
			free(p);
			return -1;
		}
		free(p);
		script = filenamebuf;
	}

	//initialise graphics
	if (!graphics_Init(&error)) {
		printerror("Error when initialising graphics: %s\n",error);
		free(error);
		return -1;
	}
	
	//open and run provided file
	if (!luastate_DoInitialFile(script, &error)) {
		if (error == NULL) {
			error = outofmem;
		}
		printerror("Error when running \"%s\": %s\n",script,error);
		if (filenamebuf) {free(filenamebuf);}
		return -1;
	}

	//call init
	if (!luastate_CallFunctionInMainstate("blitwiz.callback.load", 0, 1, 1, &error)) {
		printerror("Error when calling blitwiz.callback.load: %s\n",error);
		return -1;
	}
	
	//when graphics are open, run the main loop
	if (graphics_AreGraphicsRunning()) {
		uint64_t logictimestamp = time_GetMilliSeconds();
		uint64_t lastdrawingtime = 0;
		while (!wantquit) {
			uint64_t time = time_GetMilliSeconds();

			//first, call the step function
			while (logictimestamp < time) {
				if (!luastate_CallFunctionInMainstate("blitwiz.callback.step", 0, 1, 1, &error)) {
					printerror("Error when calling blitwiz.callback.step: %s\n", error);
					if (error) {free(error);}
				}
				logictimestamp += TIMESTEP;
			}

			//check for image loading progress
			graphics_CheckTextureLoading(&imgloaded);	

			//limit to roughly 60 FPS
			uint64_t delta = time_GetMilliSeconds()-lastdrawingtime;
			if (delta < 15) {
				time_Sleep(16-delta);
			}
			
			//start drawing
			drawingallowed = 1;
			graphics_StartFrame();
			
			//call the drawing function
			if (!luastate_CallFunctionInMainstate("blitwiz.callback.draw", 0, 1, 1, &error)) {
				printerror("Error when calling blitwiz.callback.draw: %s\n",error);
				if (error) {free(error);}
			}
			
			//complete the drawing
			drawingallowed = 0;
			graphics_CompleteFrame();
		}
	}
	return 0;
}

