/*
** LINUX_VKIMP.C
**
** Linux X11/Xlib Vulkan window + driver init (Step 1 bring-up).
** Mirrors win32/win_vkimp.c: create window, VK_Setup(dpy, win),
** fill glConfig strings. Full input/gamma handled by existing
** linux_glimp.c path for now.
*/

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "../renderer/tr_local.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "unix_glw.h"
#include "linux_local.h"

qboolean QVK_Init( const char *dllname );
void QVK_Shutdown( void );
void VK_Setup( void *p1, void *p2 );
void VK_GetDeviceProperties( VkPhysicalDeviceProperties *devProperties );

static Display *vk_dpy = NULL;
static Window vk_win = 0;

glwstate_t glw_state;

static qboolean VKW_CreateWindow( int width, int height )
{
	int scrnum;
	Window root;
	XSetWindowAttributes attr;
	unsigned long mask;

	vk_dpy = XOpenDisplay( NULL );
	if ( !vk_dpy ) {
		ri.Printf( PRINT_ALL, "VKW_CreateWindow: couldn't open X display\n" );
		return qfalse;
	}

	scrnum = DefaultScreen( vk_dpy );
	root = RootWindow( vk_dpy, scrnum );

	attr.background_pixel = BlackPixel( vk_dpy, scrnum );
	attr.border_pixel = 0;
	attr.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
		ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
	mask = CWBackPixel | CWBorderPixel | CWEventMask;

	vk_win = XCreateWindow( vk_dpy, root, 0, 0, width, height, 0,
		CopyFromParent, InputOutput, CopyFromParent, mask, &attr );
	if ( !vk_win ) {
		ri.Printf( PRINT_ALL, "VKW_CreateWindow: XCreateWindow failed\n" );
		XCloseDisplay( vk_dpy );
		vk_dpy = NULL;
		return qfalse;
	}

	XStoreName( vk_dpy, vk_win, "Quake 3: Arena (Vulkan)" );
	XMapWindow( vk_dpy, vk_win );
	XFlush( vk_dpy );

	ri.Printf( PRINT_ALL, "...created Vulkan window (%dx%d)\n", width, height );
	return qtrue;
}

static qboolean VKW_InitDriver( const char *drivername, int colorbits )
{
	(void)drivername;
	(void)colorbits;
	VK_Setup( (void *)vk_dpy, (void *)(uintptr_t)vk_win );
	return qtrue;
}

void VKimp_Init( void )
{
	int width = 1280, height = 720;
	float aspect;
	cvar_t *lastValidRenderer;

	ri.Printf( PRINT_ALL, "Initializing Vulkan subsystem (Linux/Xlib)\n" );

	InitSig();

	lastValidRenderer = ri.Cvar_Get( "r_lastValidRenderer", "(uninitialized)", CVAR_ARCHIVE );

	if ( R_GetModeInfo( &width, &height, &aspect, r_mode->integer ) ) {
		glConfig.vidWidth = width;
		glConfig.vidHeight = height;
		glConfig.windowAspect = aspect;
	} else {
		ri.Printf( PRINT_ALL, "...invalid r_mode %d, using 1280x720\n", r_mode->integer );
		glConfig.vidWidth = width;
		glConfig.vidHeight = height;
	}

	if ( !QVK_Init( VULKAN_DRIVER_NAME ) ) {
		ri.Error( ERR_FATAL, "VKimp_Init() - could not load Vulkan library %s\n", VULKAN_DRIVER_NAME );
	}

	if ( !VKW_CreateWindow( glConfig.vidWidth, glConfig.vidHeight ) ) {
		QVK_Shutdown();
		ri.Error( ERR_FATAL, "VKimp_Init() - could not create Vulkan window\n" );
	}

	if ( !VKW_InitDriver( VULKAN_DRIVER_NAME, r_colorbits->integer ) ) {
		QVK_Shutdown();
		ri.Error( ERR_FATAL, "VKimp_Init() - could not init Vulkan driver\n" );
	}

	{
		VkPhysicalDeviceProperties deviceProp;
		const char *vendor_name = "unknown";
		char version[32];

		VK_GetDeviceProperties( &deviceProp );

		if ( deviceProp.vendorID == 0x1002 ) {
			vendor_name = "Advanced Micro Devices, Inc.";
		} else if ( deviceProp.vendorID == 0x10DE ) {
			vendor_name = "NVIDIA Corporation";
		} else if ( deviceProp.vendorID == 0x8086 ) {
			vendor_name = "Intel Corporation";
		}
		Q_strncpyz( glConfig.vendor_string, vendor_name, sizeof( glConfig.vendor_string ) );
		Q_strncpyz( glConfig.renderer_string, (const char *)deviceProp.deviceName, sizeof( glConfig.renderer_string ) );

		Com_sprintf( version, sizeof( version ), "%d.%d.%d",
			VK_VERSION_MAJOR( deviceProp.apiVersion ),
			VK_VERSION_MINOR( deviceProp.apiVersion ),
			VK_VERSION_PATCH( deviceProp.apiVersion ) );
		Q_strncpyz( glConfig.version_string, version, sizeof( glConfig.version_string ) );
		glConfig.extensions_string[0] = '\0';
	}

	if ( vk.swapchain.imageFormat == VK_FORMAT_B8G8R8A8_UNORM ) {
		glConfig.colorBits = 32;
	}
	if ( vk.swapchain.depthStencilFormat == VK_FORMAT_D24_UNORM_S8_UINT ) {
		glConfig.depthBits = 24;
		glConfig.stencilBits = 8;
	}

	glConfig.textureEnvAddAvailable = qtrue;

	ri.Cvar_Set( "r_lastValidRenderer", glConfig.renderer_string );
	(void)lastValidRenderer;
}

void VKimp_Shutdown( void )
{
	ri.Printf( PRINT_ALL, "Shutting down Vulkan subsystem (Linux/Xlib)\n" );

	if ( vk_dpy ) {
		if ( vk_win ) {
			XDestroyWindow( vk_dpy, vk_win );
			vk_win = 0;
		}
		XCloseDisplay( vk_dpy );
		vk_dpy = NULL;
	}

	QVK_Shutdown();

	memset( &glConfig, 0, sizeof( glConfig ) );
	memset( &glState, 0, sizeof( glState ) );
}

/*
** LINUX INPUT + SMP (moved from linux_glimp.c during Vulkan-only port;
** XF86DGA/XF86VidMode paths removed, plain X11/GLX-free operation)
*/
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include <X11/keysym.h>
#include <X11/cursorfont.h>

int Sys_XTimeToSysTime(unsigned long xtime);

#define KEY_MASK (KeyPressMask | KeyReleaseMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | \
		    PointerMotionMask | ButtonMotionMask )
#define X_MASK (KEY_MASK | MOUSE_MASK | VisibilityChangeMask | StructureNotifyMask )

static qboolean mouse_avail;
static qboolean mouse_active = qfalse;
static int mwx, mwy;
static int mx = 0, my = 0;

// Time mouse was reset, we ignore the first 50ms of the mouse to allow settling of events
static int mouseResetTime = 0;
#define MOUSE_RESET_DELAY 50

static cvar_t *in_mouse;
cvar_t *in_subframe;
cvar_t *in_nograb; // this is strictly for developers

// bk001130 - from cvs.17 (mkv), joystick variables
cvar_t   *in_joystick      = NULL;
cvar_t   *in_joystickDebug = NULL;
cvar_t   *joy_threshold    = NULL;

static int mouse_accel_numerator;
static int mouse_accel_denominator;
static int mouse_threshold;

/*****************************************************************************
** KEYBOARD
** NOTE TTimo the keyboard handling is done with KeySyms
**   that means relying on the keyboard mapping provided by X
**   in-game it would probably be better to use KeyCode (i.e. hardware key codes)
**   you would still need the KeySyms in some cases, such as for the console and all entry textboxes
**     (cause there's nothing worse than a qwerty mapping on a french keyboard)
**
** you can turn on some debugging and verbose of the keyboard code with #define KBD_DBG
******************************************************************************/

//#define KBD_DBG

static char *XLateKey(XKeyEvent *ev, int *key)
{
  static char buf[64];
  KeySym keysym;
  int XLookupRet;

  *key = 0;

  XLookupRet = XLookupString(ev, buf, sizeof buf, &keysym, 0);
#ifdef KBD_DBG
  ri.Printf(PRINT_ALL, "XLookupString ret: %d buf: %s keysym: %x\n", XLookupRet, buf, keysym);
#endif
  
  switch (keysym)
  {
  case XK_KP_Page_Up: 
  case XK_KP_9:  *key = K_KP_PGUP; break;
  case XK_Page_Up:   *key = K_PGUP; break;

  case XK_KP_Page_Down: 
  case XK_KP_3: *key = K_KP_PGDN; break;
  case XK_Page_Down:   *key = K_PGDN; break;

  case XK_KP_Home: *key = K_KP_HOME; break;
  case XK_KP_7: *key = K_KP_HOME; break;
  case XK_Home:  *key = K_HOME; break;

  case XK_KP_End:
  case XK_KP_1:   *key = K_KP_END; break;
  case XK_End:   *key = K_END; break;

  case XK_KP_Left: *key = K_KP_LEFTARROW; break;
  case XK_KP_4: *key = K_KP_LEFTARROW; break;
  case XK_Left:  *key = K_LEFTARROW; break;

  case XK_KP_Right: *key = K_KP_RIGHTARROW; break;
  case XK_KP_6: *key = K_KP_RIGHTARROW; break;
  case XK_Right:  *key = K_RIGHTARROW;    break;

  case XK_KP_Down:
  case XK_KP_2:    *key = K_KP_DOWNARROW; break;
  case XK_Down:  *key = K_DOWNARROW; break;

  case XK_KP_Up:   
  case XK_KP_8:    *key = K_KP_UPARROW; break;
  case XK_Up:    *key = K_UPARROW;   break;

  case XK_Escape: *key = K_ESCAPE;    break;

  case XK_KP_Enter: *key = K_KP_ENTER;  break;
  case XK_Return: *key = K_ENTER;    break;

  case XK_Tab:    *key = K_TAB;      break;

  case XK_F1:    *key = K_F1;       break;

  case XK_F2:    *key = K_F2;       break;

  case XK_F3:    *key = K_F3;       break;

  case XK_F4:    *key = K_F4;       break;

  case XK_F5:    *key = K_F5;       break;

  case XK_F6:    *key = K_F6;       break;

  case XK_F7:    *key = K_F7;       break;

  case XK_F8:    *key = K_F8;       break;

  case XK_F9:    *key = K_F9;       break;

  case XK_F10:    *key = K_F10;      break;

  case XK_F11:    *key = K_F11;      break;

  case XK_F12:    *key = K_F12;      break;

    // bk001206 - from Ryan's Fakk2 
    //case XK_BackSpace: *key = 8; break; // ctrl-h
  case XK_BackSpace: *key = K_BACKSPACE; break; // ctrl-h

  case XK_KP_Delete:
  case XK_KP_Decimal: *key = K_KP_DEL; break;
  case XK_Delete: *key = K_DEL; break;

  case XK_Pause:  *key = K_PAUSE;    break;

  case XK_Shift_L:
  case XK_Shift_R:  *key = K_SHIFT;   break;

  case XK_Execute: 
  case XK_Control_L: 
  case XK_Control_R:  *key = K_CTRL;  break;

  case XK_Alt_L:  
  case XK_Meta_L: 
  case XK_Alt_R:  
  case XK_Meta_R: *key = K_ALT;     break;

  case XK_KP_Begin: *key = K_KP_5;  break;

  case XK_Insert:   *key = K_INS; break;
  case XK_KP_Insert:
  case XK_KP_0: *key = K_KP_INS; break;

  case XK_KP_Multiply: *key = '*'; break;
  case XK_KP_Add:  *key = K_KP_PLUS; break;
  case XK_KP_Subtract: *key = K_KP_MINUS; break;
  case XK_KP_Divide: *key = K_KP_SLASH; break;

    // bk001130 - from cvs1.17 (mkv)
  case XK_exclam: *key = '1'; break;
  case XK_at: *key = '2'; break;
  case XK_numbersign: *key = '3'; break;
  case XK_dollar: *key = '4'; break;
  case XK_percent: *key = '5'; break;
  case XK_asciicircum: *key = '6'; break;
  case XK_ampersand: *key = '7'; break;
  case XK_asterisk: *key = '8'; break;
  case XK_parenleft: *key = '9'; break;
  case XK_parenright: *key = '0'; break;
  
  // weird french keyboards ..
  // NOTE: console toggle is hardcoded in cl_keys.c, can't be unbound
  //   cleaner would be .. using hardware key codes instead of the key syms
  //   could also add a new K_KP_CONSOLE
  case XK_twosuperior: *key = '~'; break;
		
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=472
	case XK_space:
	case XK_KP_Space: *key = K_SPACE; break;

  default:
    if (XLookupRet == 0)
    {
      if (com_developer->value)
      {
        ri.Printf(PRINT_ALL, "Warning: XLookupString failed on KeySym %d\n", keysym);
      }
      return NULL;
    }
    else
    {
      // XK_* tests failed, but XLookupString got a buffer, so let's try it
      *key = *(unsigned char *)buf;
      if (*key >= 'A' && *key <= 'Z')
        *key = *key - 'A' + 'a';
      // if ctrl is pressed, the keys are not between 'A' and 'Z', for instance ctrl-z == 26 ^Z ^C etc.
      // see https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=19
      else if (*key >= 1 && *key <= 26)
     	  *key = *key + 'a' - 1;
    }
    break;
  } 

  return buf;
}

static Cursor CreateNullCursor(Display *display, Window root)
{
  Pixmap cursormask; 
  XGCValues xgc;
  GC gc;
  XColor dummycolour;
  Cursor cursor;

  cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
  xgc.function = GXclear;
  gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
  XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
  dummycolour.pixel = 0;
  dummycolour.red = 0;
  dummycolour.flags = 04;
  cursor = XCreatePixmapCursor(display, cursormask, cursormask,
                               &dummycolour,&dummycolour, 0,0);
  XFreePixmap(display,cursormask);
  XFreeGC(display,gc);
  return cursor;
}

static void install_grabs(void)
{
  // inviso cursor
  XWarpPointer(vk_dpy, None, vk_win,
               0, 0, 0, 0,
               glConfig.vidWidth / 2, glConfig.vidHeight / 2);
  XSync(vk_dpy, False);

  XDefineCursor(vk_dpy, vk_win, CreateNullCursor(vk_dpy, vk_win));

  XGrabPointer(vk_dpy, vk_win, // bk010108 - do this earlier?
               False,
               MOUSE_MASK,
               GrabModeAsync, GrabModeAsync,
               vk_win,
               None,
               CurrentTime);

  XGetPointerControl(vk_dpy, &mouse_accel_numerator, &mouse_accel_denominator,
                     &mouse_threshold);

  XChangePointerControl(vk_dpy, True, True, 1, 1, 0);

  XSync(vk_dpy, False);

  mouseResetTime = Sys_Milliseconds ();

  mwx = glConfig.vidWidth / 2;
  mwy = glConfig.vidHeight / 2;
  mx = my = 0;

  XGrabKeyboard(vk_dpy, vk_win,
                False,
                GrabModeAsync, GrabModeAsync,
                CurrentTime);

  XSync(vk_dpy, False);
}

static void uninstall_grabs(void)
{
  XChangePointerControl(vk_dpy, qtrue, qtrue, mouse_accel_numerator, 
                        mouse_accel_denominator, mouse_threshold);

  XUngrabPointer(vk_dpy, CurrentTime);
  XUngrabKeyboard(vk_dpy, CurrentTime);

  XWarpPointer(vk_dpy, None, vk_win,
               0, 0, 0, 0,
               glConfig.vidWidth / 2, glConfig.vidHeight / 2);

  // inviso cursor
  XUndefineCursor(vk_dpy, vk_win);
}

static qboolean X11_PendingInput(void) {

  assert(vk_dpy != NULL);

  // Flush the display connection
  //  and look to see if events are queued
  XFlush( vk_dpy );
  if ( XEventsQueued( vk_dpy, QueuedAlready) )
  {
    return qtrue;
  }

  // More drastic measures are required -- see if X is ready to talk
  {
    static struct timeval zero_time;
    int x11_fd;
    fd_set fdset;

    x11_fd = ConnectionNumber( vk_dpy );
    FD_ZERO(&fdset);
    FD_SET(x11_fd, &fdset);
    if ( select(x11_fd+1, &fdset, NULL, NULL, &zero_time) == 1 )
    {
      return(XPending(vk_dpy));
    }
  }

  // Oh well, nothing is ready ..
  return qfalse;
}

static qboolean repeated_press(XEvent *event)
{
  XEvent        peekevent;
  qboolean      repeated = qfalse;

  assert(vk_dpy != NULL);

  if (X11_PendingInput())
  {
    XPeekEvent(vk_dpy, &peekevent);

    if ((peekevent.type == KeyPress) &&
        (peekevent.xkey.keycode == event->xkey.keycode) &&
        (peekevent.xkey.time == event->xkey.time))
    {
      repeated = qtrue;
      XNextEvent(vk_dpy, &peekevent);  // skip event.
    } // if
  } // if

  return(repeated);
}

static void HandleEvents(void)
{
  int b;
  int key;
  XEvent event;
  qboolean dowarp = qfalse;
  char *p;
  int dx, dy;
  int t = 0; // default to 0 in case we don't set
	
  if (!vk_dpy)
    return;

  while (XPending(vk_dpy))
  {
    XNextEvent(vk_dpy, &event);
    switch (event.type)
    {
    case KeyPress:
			t = Sys_XTimeToSysTime(event.xkey.time);
      p = XLateKey(&event.xkey, &key);
      if (key)
      {
        Sys_QueEvent( t, SE_KEY, key, qtrue, 0, NULL );
      }
      if (p)
      {
        while (*p)
        {
          Sys_QueEvent( t, SE_CHAR, *p++, 0, 0, NULL );
        }
      }
      break;

    case KeyRelease:
			t = Sys_XTimeToSysTime(event.xkey.time);
      // bk001206 - handle key repeat w/o XAutRepatOn/Off
      //            also: not done if console/menu is active.
      // From Ryan's Fakk2.
      // see game/q_shared.h, KEYCATCH_* . 0 == in 3d game.  
      if (cls.keyCatchers == 0)
      {   // FIXME: KEYCATCH_NONE
        if (repeated_press(&event) == qtrue)
          continue;
      } // if
      XLateKey(&event.xkey, &key);

      Sys_QueEvent( t, SE_KEY, key, qfalse, 0, NULL );
      break;

    case MotionNotify:
			t = Sys_XTimeToSysTime(event.xkey.time);
      if (mouse_active)
      {
          // If it's a center motion, we've just returned from our warp
          if (event.xmotion.x == glConfig.vidWidth/2 &&
              event.xmotion.y == glConfig.vidHeight/2)
          {
            mwx = glConfig.vidWidth/2;
            mwy = glConfig.vidHeight/2;
            if (t - mouseResetTime > MOUSE_RESET_DELAY )
            {
              Sys_QueEvent( t, SE_MOUSE, mx, my, 0, NULL );
            }
            mx = my = 0;
            break;
          }

          dx = ((int)event.xmotion.x - mwx);
          dy = ((int)event.xmotion.y - mwy);
          if (abs(dx) > 1)
            mx += dx * 2;
          else
            mx += dx;
          if (abs(dy) > 1)
            my += dy * 2;
          else
            my += dy;

          mwx = event.xmotion.x;
          mwy = event.xmotion.y;
          dowarp = qtrue;
      }
      break;

    case ButtonPress:
		  t = Sys_XTimeToSysTime(event.xkey.time);
      if (event.xbutton.button == 4)
      {
        Sys_QueEvent( t, SE_KEY, K_MWHEELUP, qtrue, 0, NULL );
      } else if (event.xbutton.button == 5)
      {
        Sys_QueEvent( t, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL );
      } else
      {
        // NOTE TTimo there seems to be a weird mapping for K_MOUSE1 K_MOUSE2 K_MOUSE3 ..
        b=-1;
        if (event.xbutton.button == 1)
        {
          b = 0; // K_MOUSE1
        } else if (event.xbutton.button == 2)
        {
          b = 2; // K_MOUSE3
        } else if (event.xbutton.button == 3)
        {
          b = 1; // K_MOUSE2
        } else if (event.xbutton.button == 6)
        {
          b = 3; // K_MOUSE4
        } else if (event.xbutton.button == 7)
        {
          b = 4; // K_MOUSE5
        };

        Sys_QueEvent( t, SE_KEY, K_MOUSE1 + b, qtrue, 0, NULL );
      }
      break;

    case ButtonRelease:
		  t = Sys_XTimeToSysTime(event.xkey.time);
      if (event.xbutton.button == 4)
      {
        Sys_QueEvent( t, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
      } else if (event.xbutton.button == 5)
      {
        Sys_QueEvent( t, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
      } else
      {
        b=-1;
        if (event.xbutton.button == 1)
        {
          b = 0;
        } else if (event.xbutton.button == 2)
        {
          b = 2;
        } else if (event.xbutton.button == 3)
        {
          b = 1;
        } else if (event.xbutton.button == 6)
        {
          b = 3; // K_MOUSE4
        } else if (event.xbutton.button == 7)
        {
          b = 4; // K_MOUSE5
        };
        Sys_QueEvent( t, SE_KEY, K_MOUSE1 + b, qfalse, 0, NULL );
      }
      break;

    }
  }

  if (dowarp)
  {
    XWarpPointer(vk_dpy,None,vk_win,0,0,0,0, 
                 (glConfig.vidWidth/2),(glConfig.vidHeight/2));
  }
}

void KBD_Init(void)
{
}

void KBD_Close(void)
{
}

void IN_ActivateMouse( void ) 
{
  if (!mouse_avail || !vk_dpy || !vk_win)
    return;

  if (!mouse_active)
  {
		if (!in_nograb->value)
      install_grabs();
    mouse_active = qtrue;
  }
}

void IN_DeactivateMouse( void ) 
{
  if (!mouse_avail || !vk_dpy || !vk_win)
    return;

  if (mouse_active)
  {
		if (!in_nograb->value)
      uninstall_grabs();
    mouse_active = qfalse;
  }
}

void IN_Init(void) {
	Com_Printf ("\n------- Input Initialization -------\n");
  // mouse variables
  in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE);
	
	// turn on-off sub-frame timing of X events
	in_subframe = Cvar_Get ("in_subframe", "1", CVAR_ARCHIVE);
	
	// developer feature, allows to break without loosing mouse pointer
	in_nograb = Cvar_Get ("in_nograb", "0", 0);

  // bk001130 - from cvs.17 (mkv), joystick variables
  in_joystick = Cvar_Get ("in_joystick", "0", CVAR_ARCHIVE|CVAR_LATCH);
  // bk001130 - changed this to match win32
  in_joystickDebug = Cvar_Get ("in_debugjoystick", "0", CVAR_TEMP);
  joy_threshold = Cvar_Get ("joy_threshold", "0.15", CVAR_ARCHIVE); // FIXME: in_joythreshold

  if (in_mouse->value)
    mouse_avail = qtrue;
  else
    mouse_avail = qfalse;

  IN_StartupJoystick( ); // bk001130 - from cvs1.17 (mkv)
	Com_Printf ("------------------------------------\n");
}

void IN_Shutdown(void)
{
  mouse_avail = qfalse;
}

void IN_Frame (void) {

  // bk001130 - from cvs 1.17 (mkv)
  IN_JoyMove(); // FIXME: disable if on desktop?

  if ( cls.keyCatchers & KEYCATCH_CONSOLE )
  {
    // temporarily deactivate if not in the game and
    // running on the desktop
    // voodoo always counts as full screen
    if (Cvar_VariableValue ("r_fullscreen") == 0)
    {
      IN_DeactivateMouse ();
      return;
    }
  }

  IN_ActivateMouse();
}

void IN_Activate(void)
{
}

void Sys_SendKeyEvents (void) {
  // XEvent event; // bk001204 - unused

  if (!vk_dpy)
    return;
  HandleEvents();
}

void GLimp_RenderThreadWrapper( void *stub ) {}
qboolean GLimp_SpawnRenderThread( void (*function)( void ) ) {
	ri.Printf( PRINT_WARNING, "ERROR: SMP support was disabled at compile time\n");
  return qfalse;
}
void *GLimp_RendererSleep( void ) {
  return NULL;
}
void GLimp_FrontEndSleep( void ) {}
void GLimp_WakeRenderer( void *data ) {}

