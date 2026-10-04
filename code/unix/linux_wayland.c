/*
===========================================================================
linux_wayland.c — minimal Wayland window backend for the Vulkan-only fork
Tries a Wayland window first; on any failure the X11 path is used.
Wire into the Vulkan surface creation as VkWaylandSurfaceCreateInfoKHR.
===========================================================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <wayland-client.h>
#include "xdg/xdg-shell-client-protocol.h"

static struct wl_display *wl_dpy;
static struct wl_registry *wl_reg;
static struct wl_compositor *wil_compositor;
static struct wl_surface *wl_surf;
static struct xdg_wm_base *xdg_wm;
static struct xdg_surface *xdg_surf;
static struct xdg_toplevel *xdg_top;
static int wl_configured_size;
static int wl_expected_width, wl_expected_height;
static int wayland_ok;

static void registry_add(void *data, struct wl_registry *reg, uint32_t name, const char *iface, uint32_t version) {
    if (strcmp(iface, "wl_compositor") == 0) {
        wil_compositor = wl_registry_bind(reg, name, &wl_compositor_interface, version);
    } else if (strcmp(iface, "xdg_wm_base") == 0) {
        xdg_wm = wl_registry_bind(reg, name, &xdg_wm_base_interface, version);
    }
}
static void registry_remove(void *data, struct wl_registry *reg, uint32_t name) { (void)data; (void)reg; (void)name; }
static const struct wl_registry_listener reg_listener = { .global = registry_add, .global_remove = registry_remove };

static void wm_ping(void *data, struct xdg_wm_base *wm, uint32_t serial) { xdg_wm_base_pong(wm, serial); }
static const struct xdg_wm_base_listener wm_listener = { .ping = wm_ping };

static void surf_configure(void *data, struct xdg_surface *surf, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(surf, serial);
    wl_surface_commit(wl_surf);
}
static const struct xdg_surface_listener surf_listener = { .configure = surf_configure };

static void top_configure(void *data, struct xdg_toplevel *top, int32_t w, int32_t h, struct wl_array *states) {
    (void)data; (void)top; (void)states;
    if (w == 0 || h == 0) { w = wl_expected_width; h = wl_expected_height; }
    wl_configured_size = 1;
    wl_expected_width = w; wl_expected_height = h;
}
static void top_close(void *data, struct xdg_toplevel *top) { (void)data; (void)top; }
static void top_configure_bounds(void *data, struct xdg_toplevel *top, int32_t x, int32_t y) { (void)data; (void)top; (void)x; (void)y; }
static void top_wm_capabilities(void *data, struct xdg_toplevel *top, struct wl_array *caps) { (void)data; (void)top; (void)caps; }
static const struct xdg_toplevel_listener top_listener = {
    .configure = top_configure,
    .close = top_close,
    .configure_bounds = top_configure_bounds,
    .wm_capabilities = top_wm_capabilities,
};

int VKW_CreateWaylandWindow(int width, int height) {
    wl_expected_width = width; wl_expected_height = height;
    if (!getenv("WAYLAND_DISPLAY")) return 0;
    wl_dpy = wl_display_connect(NULL);
    if (!wl_dpy) return 0;
    wl_reg = wl_display_get_registry(wl_dpy);
    wl_registry_add_listener(wl_reg, &reg_listener, NULL);
    wl_display_roundtrip(wl_dpy);
    if (!wil_compositor || !xdg_wm) { wl_display_disconnect(wl_dpy); wl_dpy = NULL; return 0; }
    xdg_wm_base_add_listener(xdg_wm, &wm_listener, NULL);
    wl_surf = wl_compositor_create_surface(wil_compositor);
    xdg_surf = xdg_wm_base_get_xdg_surface(xdg_wm, wl_surf);
    xdg_surface_add_listener(xdg_surf, &surf_listener, NULL);
    xdg_top = xdg_surface_get_toplevel(xdg_surf);
    xdg_toplevel_add_listener(xdg_top, &top_listener, NULL);
    xdg_toplevel_set_title(xdg_top, "Quake 3: Arena (Vulkan/Wayland)");
    wl_surface_commit(wl_surf);
    wl_display_roundtrip(wl_dpy);
    wayland_ok = 1;
    return 1;
}

struct wl_display *VKimpGetWaylandDisplay(void) { return wayland_ok ? wl_dpy : NULL; }
struct wl_surface *VKimpGetWaylandSurface(void) { return wayland_ok ? wl_surf : NULL; }
void VKimpWaylandPump(void) { if (wayland_ok) wl_display_flush(wl_dpy), wl_display_dispatch_pending(wl_dpy); }
