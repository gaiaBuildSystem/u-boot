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

#define READ_OF_NODE(key, param) {				\
	ofnode_read_u32(node, #param, &value);			\
	key = value;						\
}
DECLARE_GLOBAL_DATA_PTR;

static struct udevice *backlight;
static struct udevice *regulator;

typedef struct cmd_tbl_s	cmd_tbl_t;
struct gpio_desc enable_gpio;

static int berlin_fb_sync(struct udevice *dev)
{
	struct berlin_fb_priv *priv = dev_get_priv(dev);
	VBUF_INFO *pVppBuf = NULL;

	return MV_VPP_Display_Frame(priv, pVppBuf, DISPLAY_1);
}

int syna_parse_lcdc_dt(struct udevice *dev)
{
	struct berlin_fb_priv *priv = dev_get_priv(dev);
	PANEL_TIMING_INFO *pTimingInfo;
	UINT32 value;
	ofnode node;

	node = dev_read_subnode(dev, "lcdc_panel");
	if (!ofnode_valid(node) || !ofnode_is_enabled(node)) {
		debug("lcdc_panel subnode not found\n");
		return 0;
	}

	priv->lcdc_config_data = malloc(sizeof(struct lcdc_config));
	if (!priv->lcdc_config_data) {
		printf("No memory for lcdc_config_data\n");
		return -ENOMEM;
	}

	dev_read_u32(dev, "irqno", &priv->lcdc_config_data->irqno);

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
	UINT32 value;
	ofnode node;
	int ret;

	ret = fdt_node_offset_by_compatible(blob, -1, "syna,mipi-dsi");
	if(ret < 0) {
		debug("mipi-dsi node not found\n");
		return 0;
	}

	node = dev_read_subnode(dev, "dsi_panel");
	if (!ofnode_is_enabled(node)) {
		debug("dsi_panel subnode not found\n");
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

	gpio_request_by_name_nodev(node, "mipirst-gpio", 0, &priv->enable,
					GPIOD_IS_OUT);
	dm_gpio_set_value(&priv->enable, 0);

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
	READ_OF_NODE(pResCfg->infoparams.tgParams.tg_vb_min, VB_MIN);
	READ_OF_NODE(pResCfg->infoparams.tgParams.tg_hb_min, HB_MIN);
	READ_OF_NODE(pResCfg->infoparams.tgParams.tg_size_v_off_p, V_OFF);
	READ_OF_NODE(pResCfg->infoparams.tgParams.tg_size_h_off_p, H_OFF);
	READ_OF_NODE(pResCfg->infoparams.tgParams.tg_hb_vop_off, HB_VOP_OFF);
	READ_OF_NODE(pResCfg->infoparams.tgParams.tg_vb0_vop_off, VB_VOP_OFF);
	READ_OF_NODE(pResCfg->infoparams.tgParams.tg_hb_be, HB_BE);
	READ_OF_NODE(pResCfg->infoparams.tgParams.tg_vb0_be, VB_BE);
	READ_OF_NODE(pResCfg->infoparams.tgParams.tg_vb_fp, VB_FP);
	READ_OF_NODE(pResCfg->infoparams.tgParams.tg_hb_fp, HB_FP);
	READ_OF_NODE(pResCfg->infoparams.tgParams.pixel_clock, PIXEL_CLOCK);
	READ_OF_NODE(pResCfg->infoparams.resInfo.width, HTOTAL);

	READ_OF_NODE(pResCfg->initparams.byte_clock, Byte_clk);
	READ_OF_NODE(pResCfg->initparams.no_of_lanes, Lanes);
	READ_OF_NODE(pResCfg->initparams.video_mode, Vid_mode);
	READ_OF_NODE(pResCfg->initparams.receive_ack_packets, Recv_ack);
	READ_OF_NODE(pResCfg->initparams.is_18_loosely, Loosely_18);
	READ_OF_NODE(pResCfg->initparams.h_polarity, H_polarity);
	READ_OF_NODE(pResCfg->initparams.v_polarity, V_Polarity);
	READ_OF_NODE(pResCfg->initparams.data_en_polarity, Data_Polarity);
	READ_OF_NODE(pResCfg->initparams.eotp_tx_en, Eotp_tx);
	READ_OF_NODE(pResCfg->initparams.eotp_rx_en, Eotp_rx);
	READ_OF_NODE(pResCfg->initparams.non_continuous_clock, non-Continuous_clk);
	READ_OF_NODE(pResCfg->initparams.dpi_lp_cmd_en, dpi_lp_cmd);
	READ_OF_NODE(pResCfg->initparams.color_coding, Color_coding);
	READ_OF_NODE(pResCfg->initparams.no_of_chunks, Chunks);
	READ_OF_NODE(pResCfg->initparams.null_packet_size, Null_Pkt);
	READ_OF_NODE(pResCfg->initparams.data_lane_polarity, Data_Lane_Polarity);
	READ_OF_NODE(pResCfg->initparams.virtual_channel, virtual_chan);
	READ_OF_NODE(pResCfg->initparams.clk_lane_polarity, Clk_Lane_Polarity);

	pResCfg->infoparams.resInfo.height = pResCfg->infoparams.resInfo.active_height +
				pResCfg->infoparams.resInfo.vfrontporch +
				pResCfg->infoparams.resInfo.vsyncwidth +
				pResCfg->infoparams.resInfo.vbackporch;

	pResCfg->vppMipiCmd.bufsize = sizeof(panel_commands);

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

	memcpy(pResCfg->vppMipiCmd.pcmd + MIPI_CMD_HEADER_SIZE, panel_commands,
	       pResCfg->vppMipiCmd.bufsize);

	pCmdHeader = (VPP_MIPI_CMD_HEADER*)pResCfg->vppMipiCmd.pcmd;
	pCmdHeader->cmd_type = VPP_CMD_TYPE_INIT;
	pCmdHeader->cmd_size = pResCfg->vppMipiCmd.bufsize;

	return 0;
}

int syna_read_config(struct udevice *dev)
{
	struct berlin_fb_priv *priv = dev_get_priv(dev);
	int ret;

	priv->vpp_config_param.display_mode = dev_read_u32_default(dev,
					       "disp-mode",
					       VOUT_DISP_SINGLE_MODE_PRI);

	priv->vpp_config_param.disp1_res_id = dev_read_u32_default(dev,
					       "disp1-res-id", RES_720P60);

	priv->vpp_config_param.disp2_res_id = dev_read_u32_default(dev,
					       "disp2-res-id", RES_DSI_CUSTOM);

	priv->vpp_config_param.disp1_bpp = dev_read_u32_default(dev,
					       "disp1-bits_per_pixel", 24);

	priv->vpp_config_param.disp1_outformat = dev_read_u32_default(dev,
						"disp2-busformat", 0);

	priv->vpp_config_param.disp2_bpp = dev_read_u32_default(dev,
					       "disp2-bits_per_pixel", 24);

	priv->vpp_config_param.disp2_outformat = dev_read_u32_default(dev,
						"disp2-busformat", 0);

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

static int berlin_fb_probe(struct udevice *dev)
{
	struct berlin_fb_priv *priv = dev_get_priv(dev);
	int ret;

	gd->flags &= ~GD_FLG_DEVINIT;

	ret = uclass_get_device(UCLASS_REGULATOR, 0,
					   &regulator);
	if (!ret) {
		regulator_set_enable(regulator, 1);
	}

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

	MV_VPP_Stop();

	return 0;
}

U_BOOT_CMD(
	show_logo, 2,	1,	do_show_logo,
	"show logo display port",
	""
);
