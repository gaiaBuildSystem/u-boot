/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Simplefb device tree support
 *
 * (C) Copyright 2015
 * Stephen Warren <swarren@wwwdotorg.org>
 */

#ifndef _FDT_SIMPLEFB_H_
#define _FDT_SIMPLEFB_H_
int fdt_simplefb_add_node(void *blob);
int fdt_simplefb_enable_and_mem_rsv(void *blob);

/**
 * fdt_simplefb_handoff() - Describe the active framebuffer to the OS
 *
 * If a display is active, this fills in and enables the devicetree's existing
 * simple-framebuffer node, or adds one under /chosen if there is none, and
 * reserves the framebuffer memory so that the OS leaves it alone. Nothing is
 * done if video is not active.
 *
 * @blob: Devicetree to be passed to the OS
 * Return: 0 if OK, -ve on error
 */
int fdt_simplefb_handoff(void *blob);
#endif
