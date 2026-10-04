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
#include "unix_glw.h"

qboolean QVK_Init( const char *dllname );
void QVK_Shutdown( void );
void VK_Setup( void *p1, void *p2 );
void VK_GetDeviceProperties( VkPhysicalDeviceProperties *devProperties );

static Display *vk_dpy = NULL;
static Window vk_win = 0;

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
