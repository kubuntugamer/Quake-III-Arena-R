/*
** LINUX_QVK.C
**
** Linux binding of Vulkan to QVK function pointers:
** QVK_Init() - loads libvulkan, resolves vkGetInstanceProcAddr
** QVK_Shutdown() - unloads library
*/

#include <dlfcn.h>

#include "../renderer/tr_local.h"
#include "unix_glw.h"

qboolean QVK_Init( const char *dllname )
{
	ri.Printf( PRINT_ALL, "...initializing QVK\n" );

	if ( glw_state.VulkanLib ) {
		ri.Printf( PRINT_ALL, "...QVK already loaded\n" );
		return qtrue;
	}

	ri.Printf( PRINT_ALL, "...calling dlopen( '%s' ): ", dllname );

	glw_state.VulkanLib = dlopen( dllname, RTLD_NOW | RTLD_GLOBAL );
	if ( !glw_state.VulkanLib ) {
		ri.Printf( PRINT_ALL, "failed: %s\n", dlerror() );
		return qfalse;
	}
	ri.Printf( PRINT_ALL, "succeeded\n" );

	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym( glw_state.VulkanLib, "vkGetInstanceProcAddr" );
	if ( !vkGetInstanceProcAddr ) {
		ri.Printf( PRINT_ALL, "...failed to load vkGetInstanceProcAddr: %s\n", dlerror() );
		dlclose( glw_state.VulkanLib );
		glw_state.VulkanLib = NULL;
		return qfalse;
	}

	return qtrue;
}

void QVK_Shutdown( void )
{
	ri.Printf( PRINT_ALL, "...shutting down QVK\n" );

	if ( glw_state.VulkanLib ) {
		ri.Printf( PRINT_ALL, "...unloading Vulkan library\n" );
		dlclose( glw_state.VulkanLib );
		glw_state.VulkanLib = NULL;
	}

	vkGetInstanceProcAddr = NULL;
}
