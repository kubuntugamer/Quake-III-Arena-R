/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
/*
=======================================================================

SYSTEM CONFIGURATION MENU

=======================================================================
*/

#include "ui_local.h"


#define ART_FRAMEL			"menu/art/frame2_l"
#define ART_FRAMER			"menu/art/frame1_r"
#define ART_BACK0			"menu/art/back_0"
#define ART_BACK1			"menu/art/back_1"

#define ID_GRAPHICS			10
#define ID_DISPLAY			11
#define ID_SOUND			12
#define ID_NETWORK			13
#define ID_ADVANCED			14
#define ID_BACK				15

#define VERTICAL_SPACING	34

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	framel;
	menubitmap_s	framer;

	menutext_s		graphics;
	menutext_s		display;
	menutext_s		sound;
	menutext_s		network;
	menutext_s		advanced;
	menubitmap_s	back;
} optionsmenu_t;

static optionsmenu_t	s_options;


/*
=================
Options_Event
=================
*/
static void Options_Event( void* ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_GRAPHICS:
		UI_GraphicsOptionsMenu();
		break;

	case ID_DISPLAY:
		UI_DisplayOptionsMenu();
		break;

	case ID_SOUND:
		UI_SoundOptionsMenu();
		break;

	case ID_NETWORK:
		UI_NetworkOptionsMenu();
		break;

	case ID_ADVANCED:
		UI_AdvancedOptionsMenu();
		break;

	case ID_BACK:
		UI_PopMenu();
		break;
	}
}


/*
===============
SystemConfig_Cache
===============
*/
void SystemConfig_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_FRAMEL );
	trap_R_RegisterShaderNoMip( ART_FRAMER );
	trap_R_RegisterShaderNoMip( ART_BACK0 );
	trap_R_RegisterShaderNoMip( ART_BACK1 );
}

/*
===============
Options_MenuInit
===============
*/
void Options_MenuInit( void ) {
	int				y;
	uiClientState_t	cstate;

	memset( &s_options, 0, sizeof(optionsmenu_t) );

	SystemConfig_Cache();
	s_options.menu.wrapAround = qtrue;

	trap_GetClientState( &cstate );
	if ( cstate.connState >= CA_CONNECTED ) {
		s_options.menu.fullscreen = qfalse;
	}
	else {
		s_options.menu.fullscreen = qtrue;
	}

	s_options.banner.generic.type	= MTYPE_BTEXT;
	s_options.banner.generic.flags	= QMF_CENTER_JUSTIFY;
	s_options.banner.generic.x		= 320;
	s_options.banner.generic.y		= 16;
	s_options.banner.string		    = "SYSTEM SETUP";
	s_options.banner.color			= color_white;
	s_options.banner.style			= UI_CENTER;

	s_options.framel.generic.type  = MTYPE_BITMAP;
	s_options.framel.generic.name  = ART_FRAMEL;
	s_options.framel.generic.flags = QMF_INACTIVE;
	s_options.framel.generic.x	   = 8;  
	s_options.framel.generic.y	   = 76;
	s_options.framel.width  	   = 256;
	s_options.framel.height  	   = 334;

	s_options.framer.generic.type  = MTYPE_BITMAP;
	s_options.framer.generic.name  = ART_FRAMER;
	s_options.framer.generic.flags = QMF_INACTIVE;
	s_options.framer.generic.x	   = 376;
	s_options.framer.generic.y	   = 76;
	s_options.framer.width  	   = 256;
	s_options.framer.height  	   = 334;

	y = 168;
	s_options.graphics.generic.type		= MTYPE_PTEXT;
	s_options.graphics.generic.flags	= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_options.graphics.generic.callback	= Options_Event;
	s_options.graphics.generic.id		= ID_GRAPHICS;
	s_options.graphics.generic.x		= 320;
	s_options.graphics.generic.y		= y;
	s_options.graphics.string			= "GRAPHICS";
	s_options.graphics.color			= color_red;
	s_options.graphics.style			= UI_CENTER;

	y += VERTICAL_SPACING;
	s_options.display.generic.type		= MTYPE_PTEXT;
	s_options.display.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_options.display.generic.callback	= Options_Event;
	s_options.display.generic.id		= ID_DISPLAY;
	s_options.display.generic.x			= 320;
	s_options.display.generic.y			= y;
	s_options.display.string			= "DISPLAY";
	s_options.display.color				= color_red;
	s_options.display.style				= UI_CENTER;

	y += VERTICAL_SPACING;
	s_options.sound.generic.type		= MTYPE_PTEXT;
	s_options.sound.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_options.sound.generic.callback	= Options_Event;
	s_options.sound.generic.id			= ID_SOUND;
	s_options.sound.generic.x			= 320;
	s_options.sound.generic.y			= y;
	s_options.sound.string				= "SOUND";
	s_options.sound.color				= color_red;
	s_options.sound.style				= UI_CENTER;

	y += VERTICAL_SPACING;
	s_options.network.generic.type		= MTYPE_PTEXT;
	s_options.network.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_options.network.generic.callback	= Options_Event;
	s_options.network.generic.id		= ID_NETWORK;
	s_options.network.generic.x			= 320;
	s_options.network.generic.y			= y;
	s_options.network.string			= "NETWORK";
	s_options.network.color				= color_red;
	s_options.network.style				= UI_CENTER;

	y += VERTICAL_SPACING;
	s_options.advanced.generic.type		= MTYPE_PTEXT;
	s_options.advanced.generic.flags	= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_options.advanced.generic.callback	= Options_Event;
	s_options.advanced.generic.id		= ID_ADVANCED;
	s_options.advanced.generic.x		= 320;
	s_options.advanced.generic.y		= y;
	s_options.advanced.string			= "ADVANCED";
	s_options.advanced.color			= color_red;
	s_options.advanced.style			= UI_CENTER;

	s_options.back.generic.type	    = MTYPE_BITMAP;
	s_options.back.generic.name     = ART_BACK0;
	s_options.back.generic.flags    = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_options.back.generic.callback = Options_Event;
	s_options.back.generic.id	    = ID_BACK;
	s_options.back.generic.x		= 0;
	s_options.back.generic.y		= 480-64;
	s_options.back.width  		    = 128;
	s_options.back.height  		    = 64;
	s_options.back.focuspic         = ART_BACK1;

	Menu_AddItem( &s_options.menu, ( void * ) &s_options.banner );
	Menu_AddItem( &s_options.menu, ( void * ) &s_options.framel );
	Menu_AddItem( &s_options.menu, ( void * ) &s_options.framer );
	Menu_AddItem( &s_options.menu, ( void * ) &s_options.graphics );
	Menu_AddItem( &s_options.menu, ( void * ) &s_options.display );
	Menu_AddItem( &s_options.menu, ( void * ) &s_options.sound );
	Menu_AddItem( &s_options.menu, ( void * ) &s_options.network );
	Menu_AddItem( &s_options.menu, ( void * ) &s_options.advanced );
	Menu_AddItem( &s_options.menu, ( void * ) &s_options.back );
}


/*
===============
UI_SystemConfigMenu
===============
*/
void UI_SystemConfigMenu( void ) {
	Options_MenuInit();
	UI_PushMenu ( &s_options.menu );
}


/*
=======================================================================

ADVANCED OPTIONS MENU

=======================================================================
*/

#define ART_FRAMEL			"menu/art/frame2_l"
#define ART_FRAMER			"menu/art/frame1_r"
#define ART_BACK0			"menu/art/back_0"
#define ART_BACK1			"menu/art/back_1"

#define ID_BACK_ADV			10

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	framel;
	menubitmap_s	framer;

	menulist_s		useUring;
	menulist_s		useUringAsync;
	menulist_s		useUringPrefetch;

	menubitmap_s	back;
} advancedOptions_t;

static advancedOptions_t	s_advancedOptions;

static const char *enabled_names[] =
{
	"Off",
	"On",
	0
};

/*
===============
AdvancedOptions_Event
===============
*/
static void AdvancedOptions_Event( void* ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_BACK_ADV:
		UI_PopMenu();
		break;
	}
}


/*
===============
AdvancedOptions_Cache
===============
*/
void AdvancedOptions_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_FRAMEL );
	trap_R_RegisterShaderNoMip( ART_FRAMER );
	trap_R_RegisterShaderNoMip( ART_BACK0 );
	trap_R_RegisterShaderNoMip( ART_BACK1 );
}


/*
===============
AdvancedOptions_SetMenuItems
===============
*/
static void AdvancedOptions_SetMenuItems( void ) {
	s_advancedOptions.useUring.curvalue = trap_Cvar_VariableValue("fs_useUring");
	s_advancedOptions.useUringAsync.curvalue = trap_Cvar_VariableValue("fs_useUringAsync");
	s_advancedOptions.useUringPrefetch.curvalue = trap_Cvar_VariableValue("fs_useUringPrefetch");
}


/*
===============
AdvancedOptions_MenuInit
===============
*/
static void AdvancedOptions_MenuInit( void ) {
	int y;

	memset( &s_advancedOptions, 0, sizeof(advancedOptions_t) );

	AdvancedOptions_Cache();
	s_advancedOptions.menu.wrapAround = qtrue;
	s_advancedOptions.menu.fullscreen = qtrue;

	s_advancedOptions.banner.generic.type  = MTYPE_BTEXT;
	s_advancedOptions.banner.generic.x	   = 320;
	s_advancedOptions.banner.generic.y	   = 16;
	s_advancedOptions.banner.string  	   = "SYSTEM SETUP";
	s_advancedOptions.banner.color         = color_white;
	s_advancedOptions.banner.style         = UI_CENTER;

	s_advancedOptions.framel.generic.type  = MTYPE_BITMAP;
	s_advancedOptions.framel.generic.name  = ART_FRAMEL;
	s_advancedOptions.framel.generic.flags = QMF_INACTIVE;
	s_advancedOptions.framel.generic.x	   = 0;
	s_advancedOptions.framel.generic.y	   = 78;
	s_advancedOptions.framel.width  	   = 256;
	s_advancedOptions.framel.height  	   = 329;

	s_advancedOptions.framer.generic.type  = MTYPE_BITMAP;
	s_advancedOptions.framer.generic.name  = ART_FRAMER;
	s_advancedOptions.framer.generic.flags = QMF_INACTIVE;
	s_advancedOptions.framer.generic.x	   = 376;
	s_advancedOptions.framer.generic.y	   = 76;
	s_advancedOptions.framer.width  	   = 256;
	s_advancedOptions.framer.height  	   = 334;

	y = 240 - 4 * (BIGCHAR_HEIGHT + 2);

	// references/modifies "fs_useUring"
	s_advancedOptions.useUring.generic.type     = MTYPE_SPINCONTROL;
	s_advancedOptions.useUring.generic.name     = "io_uring I/O:";
	s_advancedOptions.useUring.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_advancedOptions.useUring.generic.x        = 400;
	s_advancedOptions.useUring.generic.y        = y;
	s_advancedOptions.useUring.itemnames        = enabled_names;
	y += BIGCHAR_HEIGHT+2;

	// references/modifies "fs_useUringAsync"
	s_advancedOptions.useUringAsync.generic.type     = MTYPE_SPINCONTROL;
	s_advancedOptions.useUringAsync.generic.name     = "io_uring Async:";
	s_advancedOptions.useUringAsync.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_advancedOptions.useUringAsync.generic.x        = 400;
	s_advancedOptions.useUringAsync.generic.y        = y;
	s_advancedOptions.useUringAsync.itemnames        = enabled_names;
	y += BIGCHAR_HEIGHT+2;

	// references/modifies "fs_useUringPrefetch"
	s_advancedOptions.useUringPrefetch.generic.type     = MTYPE_SPINCONTROL;
	s_advancedOptions.useUringPrefetch.generic.name     = "io_uring Prefetch:";
	s_advancedOptions.useUringPrefetch.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_advancedOptions.useUringPrefetch.generic.x        = 400;
	s_advancedOptions.useUringPrefetch.generic.y        = y;
	s_advancedOptions.useUringPrefetch.itemnames        = enabled_names;
	y += 2*BIGCHAR_HEIGHT;

	s_advancedOptions.back.generic.type	    = MTYPE_BITMAP;
	s_advancedOptions.back.generic.name     = ART_BACK0;
	s_advancedOptions.back.generic.flags    = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_advancedOptions.back.generic.callback = AdvancedOptions_Event;
	s_advancedOptions.back.generic.id	    = ID_BACK_ADV;
	s_advancedOptions.back.generic.x		= 0;
	s_advancedOptions.back.generic.y		= 480-64;
	s_advancedOptions.back.width  		    = 128;
	s_advancedOptions.back.height  		    = 64;
	s_advancedOptions.back.focuspic         = ART_BACK1;

	Menu_AddItem( &s_advancedOptions.menu, ( void * ) &s_advancedOptions.banner );
	Menu_AddItem( &s_advancedOptions.menu, ( void * ) &s_advancedOptions.framel );
	Menu_AddItem( &s_advancedOptions.menu, ( void * ) &s_advancedOptions.framer );

	Menu_AddItem( &s_advancedOptions.menu, ( void * ) &s_advancedOptions.useUring );
	Menu_AddItem( &s_advancedOptions.menu, ( void * ) &s_advancedOptions.useUringAsync );
	Menu_AddItem( &s_advancedOptions.menu, ( void * ) &s_advancedOptions.useUringPrefetch );

	Menu_AddItem( &s_advancedOptions.menu, ( void * ) &s_advancedOptions.back );

	AdvancedOptions_SetMenuItems();
}


/*
===============
UI_AdvancedOptionsMenu
===============
*/
void UI_AdvancedOptionsMenu( void ) {
	AdvancedOptions_MenuInit();
	UI_PushMenu( &s_advancedOptions.menu );
}
