/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Qualcomm QCS8300 (Monaco) display for the EFI app: DSI0 + ADV7535 HDMI
 */

#ifndef __QCOM_MDSS_H
#define __QCOM_MDSS_H

#include <linux/types.h>

/**
 * struct mdss_mode - display timing
 *
 * @pclk_khz: pixel clock
 * @hact: horizontal active pixels
 * @hfp: horizontal front porch
 * @hsync: horizontal sync width
 * @hbp: horizontal back porch
 * @vact: vertical active lines
 * @vfp: vertical front porch
 * @vsync: vertical sync width
 * @vbp: vertical back porch
 */
struct mdss_mode {
	uint pclk_khz;
	uint hact, hfp, hsync, hbp;
	uint vact, vfp, vsync, vbp;
};

/* CEA-861 1280x720@60 */
extern const struct mdss_mode mdss_mode_720p;

/**
 * mdss_fb_alloc() - Allocate an XRGB8888 framebuffer the DPU can fetch
 *
 * The memory comes from EFI boot services, below 4GB since the DPU source
 * address registers are 32 bits wide and the SMMU maps it 1:1
 *
 * @m: Display mode giving the size
 * @fbp: Returns the framebuffer address
 * Return: 0 if OK, -ENOMEM if EFI could not allocate it
 */
int mdss_fb_alloc(const struct mdss_mode *m, ulong *fbp);

/**
 * mdss_display_start() - Power up and start the display pipeline
 *
 * Votes the needed RPMh resources, sets up the display clocks, DSI0 PHY and
 * controller, the DPU and the ADV7535 bridge
 *
 * @m: Display mode
 * @fb: Framebuffer from mdss_fb_alloc(), or 0 to run without one
 * @dsi_tpg: true to show the DSI test pattern instead of the framebuffer
 * Return: 0 if OK, -ve on error
 */
int mdss_display_start(const struct mdss_mode *m, ulong fb, bool dsi_tpg);

/**
 * mdss_display_stop() - Stop scanout before handing over to the OS
 *
 * Stops the DPU fetching from the framebuffer and removes the SMMU mapping
 * added for it, so that the OS display driver starts from a quiet state.
 * Only touches registers, so it is safe in an ExitBootServices() callback.
 */
void mdss_display_stop(void);

#endif
