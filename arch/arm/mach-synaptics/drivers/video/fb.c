// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016~2024 Synaptics Incorporated. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 or
 * later as published by the Free Software Foundation.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND
 * SYNAPTICS EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE, AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY
 * INTELLECTUAL PROPERTY RIGHTS. IN NO EVENT SHALL SYNAPTICS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, PUNITIVE, OR
 * CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED AND
 * BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF
 * COMPETENT JURISDICTION DOES NOT PERMIT THE DISCLAIMER OF DIRECT
 * DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS' TOTAL CUMULATIVE LIABILITY
 * TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S. DOLLARS.
 */
#include <linux/types.h>
#include <linux/compat.h>
#include <dm.h>
#include <fdtdec.h>
#include <libfdt.h>
#include <video.h>
#include <backlight.h>
#include "vpp_api.h"
#include "vpp_cfg.h"
#include "vdec_com.h"
#include <iomux.h>
#include <string.h>
#include "hal_vpp_wrap.h"
#include "vpp_priv.h"
#include "OSAL_api.h"
#include "vpp.h"
#include "panelcfg.h"
#include "command.h"
#include "videomodes.h"
#include <power/regulator.h>
#include "video_bridge.h"
#include "lt9611.h"
#include "fastboot_syna.h"
#include "misc_syna.h"
#include <dm/uclass.h>
#include <dm/device.h>
#include <dm/ofnode.h>
#include <dm/device-internal.h>
#include <dm/util.h>
#include <dm/lists.h>

#define READ_OF_NODE(key, param) {				\
	prop = fdt_getprop(blob, offset, #param, &len);		\
	if(prop && (len >= sizeof(u32))) {			\
		key = fdt32_to_cpu(prop[0]); 			\
	}							\
}

#define READ_OF_8_NODE(key, param) {			\
	prop = fdt_getprop(blob, offset, #param, &len);	\
	if(prop && (len >= sizeof(u8))) {		\
		key = prop[0]; 				\
	}						\
}

#define FDTO_SIZE 0x2000
#define FDT_MAX_SIZE 0x8000  /* Max size to increase FDT into - 32KB is usually enough */
#define BASE_DTB_WORKING_MEMORY	0x10000000 /* Memory for overlay'd DTB - hopefully safe !?!*/

#define ROOTFS_A "rootfs_a"
#define ROOTFS_B "rootfs_b"

#define DSI_PANEL_DTS_PATH	"/soc/drm/dsi_panel"
DECLARE_GLOBAL_DATA_PTR;

static struct udevice *backlight;
static struct udevice *regulator;
static struct udevice *video_bridge;

struct ctfb_res_modes video_mode;

typedef struct cmd_tbl_s	cmd_tbl_t;
struct gpio_desc enable_gpio;

static int berlin_fb_sync(struct udevice *dev)
{
	struct berlin_fb_priv *priv = dev_get_priv(dev);
	VBUF_INFO *pVppBuf = NULL;

	return MV_VPP_Display_Frame(priv, pVppBuf, DISPLAY_1);
}

extern int f_mmc_get_part_index(int mmc_dev, char *part_name);

/* Function to overlay the uboot working FDT with the DTBO
 * corresponding to the "dtbo" environment variable
 * Need to call this as early in the uboot init stage as possible
 */
int setup_uboot_fdt_overlay(void)
{
	void *fdto_addr;
	char cmd[512];
	char *s;
	int part_index, ret;
	const void *blob = gd->fdt_blob;
	void *new_fdt = (void *) BASE_DTB_WORKING_MEMORY;

	s = env_get("dtbo");

	if (!s) {
		/* no dtbo found, exit uboot fdt overlay!*/
		return -1;
	}

	fdto_addr = malloc(FDTO_SIZE);
	if (!fdto_addr) {
		printf("failed to malloc memory!\n");
		return -1;
	}

	if (0 == get_current_slot())
		part_index = f_mmc_get_part_index(get_mmc_active_dev(), ROOTFS_A);
	else
		part_index = f_mmc_get_part_index(get_mmc_active_dev(), ROOTFS_B);

	sprintf(cmd, "ext4load mmc %x:%x %p /boot/%s", get_mmc_active_dev(), part_index, fdto_addr, s);
	ret = run_command(cmd, 0);
	if (ret) {
		printf("failed to load fdto (cmd: %s)!\n", cmd);
		goto err;
	}

	ret = fdt_open_into(blob, new_fdt, FDT_MAX_SIZE);
	if (ret) {
		printf("Failed to resize FDT: %s\n", fdt_strerror(ret));
		goto err;
	}

	ret = fdt_overlay_apply(new_fdt, fdto_addr);
	if (ret) {
		printf("ERROR: Failed to apply overlay: %s\n", fdt_strerror(ret));
		goto err;
	}

	/* Now the overlay applied successfully, update global blob*/
	gd->fdt_blob = new_fdt;
err:
	free(fdto_addr);
	return ret;
}

int syna_parse_lcdc_dt(struct udevice *dev)
{
	struct berlin_fb_priv *priv = dev_get_priv(dev);
	PANEL_TIMING_INFO *pTimingInfo;
	int offset, parent_offset, len;
	const u32 *prop;
	const void *blob = gd->fdt_blob;

	priv->lcdc_config_data = malloc(sizeof(struct lcdc_config));
	if (!priv->lcdc_config_data) {
		printf("No memory for lcdc_config_data\n");
		return -ENOMEM;
	}

	/* After overlay, use fdt_ API's to work on the "live" FDT in memory*/
	if((parent_offset = fdt_path_offset(blob, "/soc/drm")) < 0) {
		printf("Parent node not found: %d\n", parent_offset);
		return 0;
	}

	prop = fdt_getprop(blob, parent_offset, "irqno", &len);
        if(!prop || (len < sizeof(u32)))
                debug("Cannot find property irqno\n");
        else
                priv->lcdc_config_data->irqno = fdt32_to_cpu(prop[0]);

	if((offset = fdt_subnode_offset(blob, parent_offset, "lcdc_panel")) < 0) {
		debug("fdt subnode offset not found for lcdc_panel\n");
		return 0;
	}

	pTimingInfo = &priv->lcdc_config_data->pTimingInfo[DISPLAY_TYPE_TFT];

	READ_OF_NODE(pTimingInfo->hact, hact);
	READ_OF_NODE(pTimingInfo->hfp, hfp);
	READ_OF_NODE(pTimingInfo->hsa, hsa);
	READ_OF_NODE(pTimingInfo->hbp, hbp);
	READ_OF_NODE(pTimingInfo->vact, vact);
	READ_OF_NODE(pTimingInfo->vfp, vfp);
	READ_OF_NODE(pTimingInfo->vsa, vsa);
	READ_OF_NODE(pTimingInfo->vbp, vbp);
	READ_OF_NODE(pTimingInfo->pixelclock, pixclockKhz);
	pTimingInfo->bpp = priv->vpp_config_param.disp1_bpp;
	pTimingInfo->outformat = priv->vpp_config_param.disp1_outformat;
	READ_OF_NODE(pTimingInfo->rgbswap, rgbswap);

	priv->lcdc_config_data->is_dev_avail[DISPLAY_TYPE_TFT] = 1;

	return 0;
}

int syna_parse_vpp_dsi_dt(struct udevice *dev)
{
	struct berlin_fb_priv *priv = dev_get_priv(dev);
	const void *blob = gd->fdt_blob;
	vpp_config_params* pMipiConfig;
	VPP_MIPI_LOAD_CONFIG *pLoadcfg;
        VPP_MIPI_CMD_HEADER  *pCmdHeader;
        VPP_MIPI_CONFIG_PARAMS *pResCfg;
	int ret;
	int cmdsize, offset, len;
	const u32 *prop;
	UINT8 *dts_panel_commands = NULL;

	ret = fdt_node_offset_by_compatible(blob, -1, "syna,mipi-dsi");
	if(ret < 0) {
		debug("mipi-dsi node not found\n");
		return 0;
	}

	/* After overlay, use fdt_ API's to work on the "live" FDT in memory*/
	if((offset = fdt_path_offset(blob, DSI_PANEL_DTS_PATH)) < 0) {
		printf("DSI node not found (%d) at %s\n", offset, DSI_PANEL_DTS_PATH);
		return 0;
	}

	pMipiConfig = &priv->vpp_config_param;

	pMipiConfig->mipi_config_params = VPP_ALLOC(sizeof(VPP_MIPI_LOAD_CONFIG));
	if (!pMipiConfig->mipi_config_params) {
		printf("mipi_config_params memory failed\n");
		return -ENOMEM;
	}
	memset(pMipiConfig->mipi_config_params, 0, sizeof(VPP_MIPI_LOAD_CONFIG));

	pMipiConfig->mipi_resinfo_params = (VPP_MIPI_CONFIG_PARAMS*)
					    VPP_ALLOC_ALLIGNED(sizeof(VPP_MIPI_CONFIG_PARAMS),
							       PAGE_SIZE);
	if (!pMipiConfig->mipi_resinfo_params) {
		printf("mipi_resinfo_params memory failed\n");
		return -ENOMEM;
	}

	ofnode node = ofnode_path(DSI_PANEL_DTS_PATH);
	if (!ofnode_valid(node)) {
		printf("Invalid ofnode for %s\n", DSI_PANEL_DTS_PATH);
	} else {
		/* If the mipirst-gpio available, set it accordingly */
		if((ret = gpio_request_by_name_nodev(node, "mipirst-gpio", 0, &priv->enable,
					GPIOD_IS_OUT)) >= 0) {
			dm_gpio_set_value(&priv->enable, 0);
		}
	}

	memset(pMipiConfig->mipi_resinfo_params, 0, sizeof(VPP_MIPI_CONFIG_PARAMS));

	pLoadcfg = pMipiConfig->mipi_config_params;
	pResCfg = pMipiConfig->mipi_resinfo_params;
	pLoadcfg->vppMipiCfgPA = (ARCH_PTR_TYPE)pMipiConfig->mipi_resinfo_params;

	READ_OF_NODE(pLoadcfg->noOfresID, NO_OF_RESID);
	READ_OF_NODE(pResCfg->initparams.resId, DSI_RES);

	READ_OF_NODE(pResCfg->infoparams.resInfo.active_width, ACTIVE_WIDTH);
	READ_OF_NODE(pResCfg->infoparams.resInfo.hfrontporch, HFP);
	READ_OF_NODE(pResCfg->infoparams.resInfo.hsyncwidth, HSYNCWIDTH);
	READ_OF_NODE(pResCfg->infoparams.resInfo.hbackporch, HBP);
	READ_OF_NODE(pResCfg->infoparams.resInfo.active_height, ACTIVE_HEIGHT);
	READ_OF_NODE(pResCfg->infoparams.resInfo.vfrontporch, VFP);
	READ_OF_NODE(pResCfg->infoparams.resInfo.vsyncwidth, VSYNCWIDTH);
	READ_OF_NODE(pResCfg->infoparams.resInfo.vbackporch, VBP);
	READ_OF_NODE(pResCfg->infoparams.resInfo.type, TYPE);
	READ_OF_NODE(pResCfg->infoparams.resInfo.scan, SCAN);
	READ_OF_NODE(pResCfg->infoparams.resInfo.frame_rate, FRAME_RATE);
	READ_OF_NODE(pResCfg->infoparams.resInfo.flag_3d, FLAG_3D);
	READ_OF_NODE(pResCfg->infoparams.resInfo.freq, FREQ);
	READ_OF_NODE(pResCfg->infoparams.resInfo.pts_per_cnt_4, PTS_PER_4);
	READ_OF_8_NODE(pResCfg->infoparams.tgParams.tg_vb_min, VB_MIN);
	READ_OF_8_NODE(pResCfg->infoparams.tgParams.tg_hb_min, HB_MIN);
	READ_OF_8_NODE(pResCfg->infoparams.tgParams.tg_size_v_off_p, V_OFF);
	READ_OF_8_NODE(pResCfg->infoparams.tgParams.tg_size_h_off_p, H_OFF);
	READ_OF_8_NODE(pResCfg->infoparams.tgParams.tg_hb_vop_off, HB_VOP_OFF);
	READ_OF_8_NODE(pResCfg->infoparams.tgParams.tg_vb0_vop_off, VB_VOP_OFF);
	READ_OF_8_NODE(pResCfg->infoparams.tgParams.tg_hb_be, HB_BE);
	READ_OF_8_NODE(pResCfg->infoparams.tgParams.tg_vb0_be, VB_BE);
	READ_OF_8_NODE(pResCfg->infoparams.tgParams.tg_vb_fp, VB_FP);
	READ_OF_8_NODE(pResCfg->infoparams.tgParams.tg_hb_fp, HB_FP);
	READ_OF_NODE(pResCfg->infoparams.tgParams.pixel_clock, PIXEL_CLOCK);
	READ_OF_NODE(pResCfg->infoparams.resInfo.width, HTOTAL);

	READ_OF_NODE(pResCfg->initparams.byte_clock, Byte_clk);
	READ_OF_8_NODE(pResCfg->initparams.no_of_lanes, Lanes);
	READ_OF_8_NODE(pResCfg->initparams.video_mode, Vid_mode);
	READ_OF_8_NODE(pResCfg->initparams.receive_ack_packets, Recv_ack);
	READ_OF_8_NODE(pResCfg->initparams.is_18_loosely, Loosely_18);
	READ_OF_8_NODE(pResCfg->initparams.h_polarity, H_polarity);
	READ_OF_8_NODE(pResCfg->initparams.v_polarity, V_Polarity);
	READ_OF_8_NODE(pResCfg->initparams.data_en_polarity, Data_Polarity);
	READ_OF_8_NODE(pResCfg->initparams.eotp_tx_en, Eotp_tx);
	READ_OF_8_NODE(pResCfg->initparams.eotp_rx_en, Eotp_rx);
	READ_OF_8_NODE(pResCfg->initparams.non_continuous_clock, non-Continuous_clk);
	READ_OF_8_NODE(pResCfg->initparams.dpi_lp_cmd_en, dpi_lp_cmd);
	READ_OF_8_NODE(pResCfg->initparams.color_coding, Color_coding);
	READ_OF_NODE(pResCfg->initparams.no_of_chunks, Chunks);
	READ_OF_NODE(pResCfg->initparams.null_packet_size, Null_Pkt);
	READ_OF_8_NODE(pResCfg->initparams.data_lane_polarity, Data_Lane_Polarity);
	READ_OF_8_NODE(pResCfg->initparams.virtual_channel, virtual_chan);
	READ_OF_8_NODE(pResCfg->initparams.clk_lane_polarity, Clk_Lane_Polarity);

	pResCfg->infoparams.resInfo.height = pResCfg->infoparams.resInfo.active_height +
				pResCfg->infoparams.resInfo.vfrontporch +
				pResCfg->infoparams.resInfo.vsyncwidth +
				pResCfg->infoparams.resInfo.vbackporch;

	/* Lets default the cmd bufsize to panelcfg.h array*/
	pResCfg->vppMipiCmd.bufsize = sizeof(panel_commands);

	/* Get the "command" buffer from the DTS */
	if((prop = fdt_getprop(blob, offset, "command", &cmdsize))) {
		/* Allocate the panel_commands array based on command array size*/
		dts_panel_commands = malloc(cmdsize);
		if(!dts_panel_commands) {
			printf("dts_panel_commands malloc failure, using default panel!\n");
		} else {
			const uint8_t *bytes = (const uint8_t *)prop;

			for (offset = 0; offset < cmdsize; offset++) {
				dts_panel_commands[offset] = bytes[offset];
			}
			/* We have the command array from DTS, update cmd bufsize accordingly */
			pResCfg->vppMipiCmd.bufsize = cmdsize;
		}
	}

	if (!pResCfg->vppMipiCmd.bufsize) {
		printf("Invalid MIPI commands\n");
		return -EINVAL;
	}

	pResCfg->vppMipiCmd.pcmd = (ARCH_PTR_TYPE*)
				   VPP_ALLOC_ALLIGNED(pResCfg->vppMipiCmd.bufsize,
						      PAGE_SIZE);
	if (!pResCfg->vppMipiCmd.pcmd) {
		printf("vppMipiCmd memory failed\n");
		return -ENOMEM;
	}

	memset(pResCfg->vppMipiCmd.pcmd, 0,
		pResCfg->vppMipiCmd.bufsize + MIPI_CMD_HEADER_SIZE);

	if(dts_panel_commands) {
		/* Use the panel commands available in the DTS */
		memcpy(pResCfg->vppMipiCmd.pcmd + MIPI_CMD_HEADER_SIZE, dts_panel_commands,
		       pResCfg->vppMipiCmd.bufsize);
	} else {
		/* Use default panel commands from panel_cfg.h*/
		memcpy(pResCfg->vppMipiCmd.pcmd + MIPI_CMD_HEADER_SIZE, panel_commands,
		       pResCfg->vppMipiCmd.bufsize);
	}

	pCmdHeader = (VPP_MIPI_CMD_HEADER*)pResCfg->vppMipiCmd.pcmd;
	pCmdHeader->cmd_type = VPP_CMD_TYPE_INIT;
	pCmdHeader->cmd_size = pResCfg->vppMipiCmd.bufsize;

	video_mode.xres = pResCfg->infoparams.resInfo.active_width;		/* visible resolution		*/
	video_mode.yres = pResCfg->infoparams.resInfo.active_height;
	video_mode.pixclock_khz = pResCfg->infoparams.tgParams.pixel_clock;	/* pixel clock in kHz           */
	video_mode.left_margin = pResCfg->infoparams.resInfo.hfrontporch;	/* time from sync to picture	*/
	video_mode.right_margin = pResCfg->infoparams.resInfo.hbackporch;	/* time from picture to sync	*/
	video_mode.upper_margin = pResCfg->infoparams.resInfo.vfrontporch;	/* time from sync to picture	*/
	video_mode.lower_margin = pResCfg->infoparams.resInfo.vbackporch;
	video_mode.hsync_len = pResCfg->infoparams.resInfo.hsyncwidth;		/* length of horizontal sync	*/
	video_mode.vsync_len = pResCfg->infoparams.resInfo.vsyncwidth;		/* length of vertical sync	*/

	return 0;
}

int syna_read_config(struct udevice *dev)
{
	struct berlin_fb_priv *priv = dev_get_priv(dev);
	int ret;
	const void *blob = gd->fdt_blob;
	int offset, len;
	const fdt32_t *prop;

	if((offset = fdt_path_offset(blob, "/soc/drm")) < 0) {
		printf("Parent node not found: %d\n", offset);
		return 0;
	}

	priv->vpp_config_param.display_mode = VOUT_DISP_SINGLE_MODE_PRI;
	priv->vpp_config_param.disp1_res_id = RES_720P60;
	priv->vpp_config_param.disp2_res_id = RES_DSI_CUSTOM;
	priv->vpp_config_param.disp1_bpp = 24;
	priv->vpp_config_param.disp1_outformat = 0;
	priv->vpp_config_param.disp2_bpp = 24;
	priv->vpp_config_param.disp2_outformat = 0;

	READ_OF_NODE(priv->vpp_config_param.display_mode, disp-mode);
	READ_OF_NODE(priv->vpp_config_param.disp1_res_id, disp1-res-id);
	READ_OF_NODE(priv->vpp_config_param.disp2_res_id, disp2-res-id);
	READ_OF_NODE(priv->vpp_config_param.disp1_bpp, disp1-bits_per_pixel);
	READ_OF_NODE(priv->vpp_config_param.disp1_outformat, disp2-busformat);
	READ_OF_NODE(priv->vpp_config_param.disp2_bpp, disp2-bits_per_pixel);
	READ_OF_NODE(priv->vpp_config_param.disp2_outformat, disp2-busformat);

	ret = syna_parse_vpp_dsi_dt(dev);
	if (ret) {
		printf("Error parsing DSI DT\n");
		return ret;
	}

	ret = syna_parse_lcdc_dt(dev);
	if (ret) {
		printf("Error parsing LCDC DT\n");
		return ret;
	}

	return ret;
}

static int berlin_fb_ofdata_to_platdata(struct udevice *dev)
{
	struct berlin_fb_priv *priv = dev_get_priv(dev);
	unsigned int node = dev_of_offset(dev);
	const void *blob = gd->fdt_blob;
	int ret;

	priv->bpix = fdtdec_get_int(blob, node, "bpix", VIDEO_BPP32);

	// channel_id = 1 => VCLK0
	// channel_id = 2 => DPICLK
	priv->channel_id = 1;

	ret = syna_read_config(dev);
	if (ret) {
		printf("Reading DTS configurations failed");
		return ret;
	}

	return 0;
}
struct driver *find_compat_driver(const char *target_compat)
{
	struct driver *drv;
	struct driver *start;
	int count, i;
	const struct udevice_id *match;

	debug("find_compat_driver, target_compat: %s\n", target_compat);
	start = ll_entry_start(struct driver, driver);
	count = ll_entry_count(struct driver, driver);

	for (i = 0; i < count; i++) {
		drv = &start[i];
		match = drv->of_match;
		if (match) {
			while (match->compatible) {
				debug("Driver '%s' supports compatible: %s\n", drv->name, match->compatible);
				if (!strcmp(match->compatible, target_compat)) {
					debug("Found matching driver [%s] for compatible [%s]\n", drv->name, match->compatible);
					return drv;
				}
				match++;
			}
		} else {
			debug("Driver '%s' has no of_match table.\n", drv->name);
		}
	}
	return NULL;
}
int probe_new_regulators(void)
{
	ofnode i2c_node, regulator_node, parent_node;
	int index = 0, ret;
	struct driver *drv;

	parent_node = ofnode_path("/soc/apb@f7e80000");

	if (!ofnode_valid(parent_node)) {
		printf("soc apb node not found\n");
		return -EINVAL;
	}

	ofnode_for_each_subnode(i2c_node, parent_node) {
		struct udevice *i2c_dev, *dev;

		/* Look for only i2c node from this parent node */
		if (!ofnode_device_is_compatible(i2c_node, "snps,designware-i2c"))
			continue;

		/* Find corresponding device for this i2c_node if available */
		if((ret = uclass_get_device_by_ofnode(UCLASS_I2C, i2c_node, &i2c_dev)))
			continue;

		/* Walk through i2c child nodes and attempt to bind any regulator node */
		ofnode_for_each_subnode(regulator_node, i2c_node) {
			if (!ofnode_valid(regulator_node))
				continue;

			const char *compat = ofnode_read_string(regulator_node, "compatible");

			/* Found valid node, look for compatible driver to probe */
			if(compat) {
				char regulator_dev_name[100];

				sprintf(regulator_dev_name, "regulator-dev%d", index);
				if((drv = find_compat_driver(compat)) != NULL) {
					/* Check if the device already bound*/
					ret = device_get_global_by_ofnode(regulator_node, &dev);
					if (!ret && !strcmp(dev->name, ofnode_get_name(regulator_node))) {
						debug("Device already bound: %s\n", dev->name);
						continue;
					}

					ret = device_bind_driver_to_node(i2c_dev, drv->name, regulator_dev_name, regulator_node, &dev);
					if(ret) {
						printf("driver %s bind to [%s/%s] failed [%d]\n", drv->name, ofnode_get_name(i2c_node),
								ofnode_get_name(regulator_node), ret);
						continue;
					} else {
						debug("driver %s bound to [%s/%s] @[%s]\n", drv->name, ofnode_get_name(i2c_node),
								ofnode_get_name(regulator_node), regulator_dev_name);
						/*Move index for next regulator binding*/
						index++;
					}

					if(dev) {
						ret = device_probe(dev);
						if(ret) {
							printf("device driver %s probe failed (%d)\n", drv->name, ret);
						} else {
							debug("device driver %s probe success!\n", drv->name);
						}
					}
				}
			} /* if(compat) */
		} /* i2c child nodes loop */
	} /* i2c parent loop */
	return 0;
}

#define MATCH_SUBSTR "panel"
int enable_all_panel_compatible_regulators(void)
{
	struct uclass *uc;
	struct udevice *dev;
	ofnode node;
	const char *compat;
	int ret;

	ret = uclass_get(UCLASS_REGULATOR, &uc);
	if (ret) {
		printf("enable_all_panel_compatible_regulators, no regulator class (%d)\n", ret);
		return ret;
	}

	uclass_foreach_dev(dev, uc) {
		node = dev_ofnode(dev);
		if (!ofnode_valid(node))
			continue;

		/* Get the compatible string */
		compat = ofnode_get_property(node, "compatible", NULL);
		if (!compat)
			continue;

		/* Match substring in compatible string */
		if (strstr(compat, MATCH_SUBSTR)) {
			/* enable the regulator */
			regulator_set_enable(dev, 1);
		}
	}
	return 0;
}
static int berlin_fb_probe(struct udevice *dev)
{
	struct berlin_fb_priv *priv = dev_get_priv(dev);
	int ret;

	gd->flags &= ~GD_FLG_DEVINIT;

	/* After dtbo overlay, we need to reprobe and bind regulator
	 * before checking the available regulators for the display */
	probe_new_regulators();

	/* Search for all panel regulators and enable */
	enable_all_panel_compatible_regulators();

	ret = MV_VPP_Init(priv);
	if (ret) {
		printf("VPP initialization failed\n");
		return -ENODEV;
	}

	ret = MV_VPP_Config_Display(priv);
	if (ret) {
		printf("VPP display configuration failed\n");
		return -ENODEV;
	}

	gd->flags |= GD_FLG_DEVINIT;

	return 0;
}

static int berlin_fb_bind(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);

	/* This is the maximum panel size we expect to see */
	plat->size = 1920 * 1080 * 4;

	return 0;
}

static int berlin_fb_remove(struct udevice *dev)
{
	struct berlin_fb_priv *priv = dev_get_priv(dev);

	MV_VPP_DeInit(priv);

	return 0;
}

static const struct video_ops berlin_fb_ops = {
	.video_sync = NULL,
};

static const struct udevice_id berlin_fb_ids[] = {
	{ .compatible = "syna,vpp-fb" },
	{ .compatible = "syna,lcdc-fb" },
	{ }
};

U_BOOT_DRIVER(berlin_fb) = {
	.name	= "berlin_fb",
	.id	= UCLASS_VIDEO,
	.of_match = berlin_fb_ids,
	.ops	= &berlin_fb_ops,
	.bind	= berlin_fb_bind,
	.probe	= berlin_fb_probe,
	.remove	= berlin_fb_remove,
	.of_to_plat	= berlin_fb_ofdata_to_platdata,
	.priv_auto = sizeof(struct berlin_fb_priv),
};

static int do_vidconsole(cmd_tbl_t *cmdtp, int flag, int argc,
			 char *const argv[])
{
	if (argc == 1)
		iomux_doenv(stdout, "uart@d000,vidconsole");
	else
		iomux_doenv(stdout, argv[1]);

	return 0;
}

U_BOOT_CMD(
	vidconsole, 2,	1,	do_vidconsole,
	"print string on video framebuffer",
	"    <string>"
);

static int syna_load_logo_push_frame(struct berlin_fb_priv *priv, int width,
				     int height, int displayID)
{
	VBUF_INFO *pVppBuf;
	int ret;

	/* Optee TA requirement, align pVppBuf memory to 4K bytes */
	pVppBuf = (VBUF_INFO*)VPP_ALLOC_ALLIGNED(sizeof(VBUF_INFO), PAGE_SIZE);
	if (!pVppBuf)
		return -ENOMEM;

	memset(pVppBuf, 0, sizeof(VBUF_INFO));

	ret = syna_load_logo_info(width, height, pVppBuf);
	if (ret != 0) {
		printf("Reading image from EMMC failed\n");
		return ret;
	}

	flush_dcache_range((uintptr_t)pVppBuf,
			   (uintptr_t)(((char *)pVppBuf) + sizeof(VBUF_INFO)));

	ret = MV_VPP_Display_Frame(priv, pVppBuf, displayID);
	if (ret) {
		printf("Failed to display logo\n");
		return ret;
	}

	printf("Loading logo %dx%d on display %d\n", width, height, displayID);
	return ret;
}

static int do_show_logo(cmd_tbl_t *cmdtp, int flag, int argc,
			char *const argv[])
{
	struct berlin_fb_priv *priv;
	int width, height, display;
	struct udevice *dev;
	int ret;

	/* Invoke the DTB overlay before video device probed */
	setup_uboot_fdt_overlay();

	if (uclass_first_device_err(UCLASS_VIDEO, &dev)) {
		printf("Video device not found\n");
		return -ENODEV;
	}

	ret = gpio_request_by_name(dev, "hdtx5v-gpio", 0, &enable_gpio,
					GPIOD_IS_OUT);
	if (ret)
		debug("%s: Could not get reset-GPIO (err = %d)\n",
		      dev->name, ret);
	else {
		ret = dm_gpio_set_value(&enable_gpio, 1);
		if (ret)
			debug("%s: Error while setting reset-GPIO (err = %d)\n",
				dev->name, ret);
	}

	priv = dev_get_priv(dev);

	for (display = 0; display < MAX_NUM_DISPLAY; display++) {
		ret = syna_get_display_modeinfo(priv, &width, &height, display);
		if (!ret) {
			ret = syna_load_logo_push_frame(priv, width, height,
							display);
			if (ret) {
				printf("Failed to display logo on display = %d\n",
					display);
				return ret;
			}
		}
	}

	MV_VPP_Enable_Interrupt(priv);

	ret = uclass_get_device(UCLASS_PANEL_BACKLIGHT, 0, &backlight);
	if (!ret)
		backlight_enable(backlight);

#ifdef CONFIG_VIDEO_BRIDGE
	ret = uclass_get_device(UCLASS_VIDEO_BRIDGE, 0, &video_bridge);
	if (!ret) {
		video_bridge_attach(video_bridge);
#ifdef CONFIG_SYNA_DRM_BRIDGE_LT9611
		lt9611_bridge_modeset(video_bridge, &video_mode);
#endif
	}
#endif

	MV_VPP_Stop();

	return 0;
}

U_BOOT_CMD(
	show_logo, 2,	1,	do_show_logo,
	"show logo display port",
	""
);
