/*
** LINUX_GAMEMODE.C
**
** Optional Feral GameMode integration (CPU governor, I/O + process
** priority, GPU scripts) via dlopen -- the binary never hard-depends
** on libgamemode, and everything is a no-op when the daemon or
** library is absent.
*/

#include <dlfcn.h>

#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"

typedef int (*gamemode_fn_t)( void );

static void *gamemodeLib = NULL;
static gamemode_fn_t gamemode_start = NULL;
static gamemode_fn_t gamemode_end = NULL;
static qboolean gamemodeActive = qfalse;

static void Sys_GameModeLoad( void ) {
	static qboolean tried = qfalse;
	if (gamemodeLib != NULL || tried) {
		return;
	}
	tried = qtrue;
	gamemodeLib = dlopen( "libgamemode.so.0", RTLD_NOW | RTLD_GLOBAL );
	if ( !gamemodeLib ) {
		return;
	}
	gamemode_start = (gamemode_fn_t)dlsym( gamemodeLib, "gamemode_request_start" );
	gamemode_end = (gamemode_fn_t)dlsym( gamemodeLib, "gamemode_request_end" );
	if ( !gamemode_start || !gamemode_end ) {
		dlclose( gamemodeLib );
		gamemodeLib = NULL;
		gamemode_start = NULL;
		gamemode_end = NULL;
	}
}

void Sys_GameModeStart( void ) {
	Sys_GameModeLoad();
	if ( gamemodeLib != NULL && !gamemodeActive ) {
		if ( gamemode_start() == 0 ) {
			gamemodeActive = qtrue;
			Com_Printf( "GameMode: performance optimisations engaged\n" );
		}
	}
}

void Sys_GameModeEnd( void ) {
	if ( gamemodeLib != NULL && gamemodeActive ) {
		gamemodeActive = qfalse;
		gamemode_end();
	}
}
