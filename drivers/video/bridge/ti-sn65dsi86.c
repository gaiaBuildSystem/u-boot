// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2025 Matheus Castello <matheus@castello.eng.br>
 *
 * TI SN65DSI86 DSI to eDP bridge driver for U-Boot
 * Based on Linux kernel driver
 * datasheet: https://www.ti.com/lit/ds/symlink/sn65dsi86.pdf
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <errno.h>
#include <i2c.h>
#include <edid.h>
#include <log.h>
#include <mipi_dsi.h>
#include <panel.h>
#include <power/regulator.h>
#include <video_bridge.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <div64.h>

/* Register addresses */
#define SN_DEVICE_REV_REG			0x08
#define SN_DPPLL_SRC_REG			0x0A
#define  DPPLL_CLK_SRC_DSICLK			BIT(0)
#define  REFCLK_FREQ_MASK			GENMASK(3, 1)
#define  REFCLK_FREQ(x)				((x) << 1)
#define  DPPLL_SRC_DP_PLL_LOCK			BIT(7)
#define SN_PLL_ENABLE_REG			0x0D
#define SN_DSI_LANES_REG			0x10
#define  CHA_DSI_LANES_MASK			GENMASK(4, 3)
#define  CHA_DSI_LANES(x)			((x) << 3)
#define SN_DSIA_CLK_FREQ_REG			0x12
#define SN_CHA_ACTIVE_LINE_LENGTH_LOW_REG	0x20
#define SN_CHA_VERTICAL_DISPLAY_SIZE_LOW_REG	0x24
#define SN_CHA_HSYNC_PULSE_WIDTH_LOW_REG	0x2C
#define SN_CHA_HSYNC_PULSE_WIDTH_HIGH_REG	0x2D
#define  CHA_HSYNC_POLARITY			BIT(7)
#define SN_CHA_VSYNC_PULSE_WIDTH_LOW_REG	0x30
#define SN_CHA_VSYNC_PULSE_WIDTH_HIGH_REG	0x31
#define  CHA_VSYNC_POLARITY			BIT(7)
#define SN_CHA_HORIZONTAL_BACK_PORCH_REG	0x34
#define SN_CHA_VERTICAL_BACK_PORCH_REG		0x36
#define SN_CHA_HORIZONTAL_FRONT_PORCH_REG	0x38
#define SN_CHA_VERTICAL_FRONT_PORCH_REG	0x3A
#define SN_CHA_TEST_PATTERN_CFG_REG		0x3C
#define  TEST_PATTERN_EN			BIT(4)
#define SN_LN_ASSIGN_REG			0x59
#define  LN_ASSIGN_WIDTH			2
#define SN_ENH_FRAME_REG			0x5A
#define  ASSR_CONTROL				BIT(0)
#define  VSTREAM_ENABLE				BIT(3)
#define  LN_POLRS_OFFSET			4
#define  LN_POLRS_MASK				0xf0
#define SN_DATA_FORMAT_REG			0x5B
#define  BPP_18_RGB				BIT(0)
#define SN_HPD_DISABLE_REG			0x5C
#define  HPD_DISABLE				BIT(0)
#define  HPD_DEBOUNCED_STATE			BIT(4)
#define SN_AUX_WDATA_REG(x)			(0x64 + (x))
#define SN_AUX_ADDR_19_16_REG			0x74
#define SN_AUX_ADDR_15_8_REG			0x75
#define SN_AUX_ADDR_7_0_REG			0x76
#define SN_AUX_LENGTH_REG			0x77
#define SN_AUX_CMD_REG				0x78
#define  AUX_CMD_SEND				BIT(0)
#define  AUX_CMD_REQ(x)				((x) << 4)
#define SN_AUX_RDATA_REG(x)			(0x79 + (x))
#define SN_SSC_CONFIG_REG			0x93
#define  DP_NUM_LANES_MASK			GENMASK(5, 4)
#define  DP_NUM_LANES(x)			((x) << 4)
#define SN_DATARATE_CONFIG_REG			0x94
#define  DP_DATARATE_MASK			GENMASK(7, 5)
#define  DP_DATARATE(x)				((x) << 5)
#define SN_TRAINING_SETTING_REG			0x95
#define  SCRAMBLE_DISABLE			BIT(4)
#define SN_ML_TX_MODE_REG			0x96
#define  ML_TX_MAIN_LINK_OFF			0
#define  ML_TX_NORMAL_MODE			BIT(0)
#define  ML_TX_SEMI_AUTO_LINK_TRAINING		BIT(1)
#define SN_IRQ_STATUS_REG			0xF0
#define  IRQ_STATUS_DSIA_ERR			BIT(0)
#define  IRQ_STATUS_PLL_UNLOCK			BIT(5)
#define SN_AUX_CMD_STATUS_REG			0xF4
#define  AUX_IRQ_STATUS_AUX_RPLY_TOUT		BIT(3)
#define  AUX_IRQ_STATUS_AUX_SHORT		BIT(5)
#define  AUX_IRQ_STATUS_NAT_I2C_FAIL		BIT(6)
#define SN_CS_DSI_ERR_STATUS_REG		0xF5
#define  DSI_ERR_SYNC_LOSS			BIT(0)
#define  DSI_ERR_CRC				BIT(1)
#define  DSI_ERR_ECC				BIT(2)
#define  DSI_ERR_INVALID_PACKET			BIT(3)

#define MIN_DSI_CLK_FREQ_MHZ			40
#define SN_MAX_DP_LANES				4
#define SN_LINK_TRAINING_TRIES			10

/* DP AUX constants from DP spec */
#define DP_MAX_LANE_COUNT			0x002
#define DP_MAX_LANE_COUNT_MASK			0x1f
#define DP_EDP_CONFIGURATION_SET		0x10a

/**
 * struct ti_sn65dsi86_priv - Private data for ti-sn65dsi86 driver
 * @dev: udevice pointer
 * @enable: GPIO descriptor for enable pin
 * @dp_lanes: Number of DP lanes to use
 * @dsi_lanes: Number of DSI lanes
 * @ln_assign: Value to program to the LN_ASSIGN register
 * @ln_polrs: Value for the 4-bit LN_POLRS field
 */
struct ti_sn65dsi86_priv {
	struct udevice *dev;
	struct gpio_desc enable;
	struct udevice *vccio_reg;
	struct udevice *vpll_reg;
	struct udevice *vcca_reg;
	struct udevice *vcc_reg;
	int dp_lanes;
	int dsi_lanes;
	unsigned int lanes;
	enum mipi_dsi_pixel_format format;
	unsigned long mode_flags;
	int bpp; /* bits per pixel derived from EDID or default */
	u8 ln_assign;
	u8 ln_polrs;
	struct display_timing timing;
	bool trained;
};

/* Clock frequencies supported by bridge in Hz (derived from REFCLK pin) */
static const u32 ti_sn_bridge_refclk_lut[] = {
	12000000,
	19200000,
	26000000,
	27000000,
	38400000,
};

/* Clock frequencies supported by bridge in Hz (derived from DACP/N pin) */
static const u32 ti_sn_bridge_dsiclk_lut[] = {
	468000000,
	384000000,
	416000000,
	486000000,
	460800000,
};

/* DP data rates in 10kHz units - matching Linux driver LUT */
static const unsigned int ti_sn_bridge_dp_rate_lut[] = {
	0, 162000, 216000, 243000, 270000, 324000, 432000, 540000
};

static const struct display_timing default_timing = {
	.pixelclock.typ		= 148500000,
	.hactive.typ		= 1920,
	.hfront_porch.typ	= 88,
	.hback_porch.typ	= 148,
	.hsync_len.typ		= 44,
	.vactive.typ		= 1080,
	.vfront_porch.typ	= 4,
	.vback_porch.typ	= 36,
	.vsync_len.typ		= 5,
};

static int ti_sn65dsi86_write(struct udevice *dev, u8 reg, u8 val)
{
	return dm_i2c_write(dev, reg, &val, 1);
}

static int ti_sn65dsi86_read(struct udevice *dev, u8 reg, u8 *val)
{
	return dm_i2c_read(dev, reg, val, 1);
}

static int ti_sn65dsi86_write_u16(struct udevice *dev, u8 reg, u16 val)
{
	u8 buf[2] = { val & 0xff, val >> 8 };
	int ret;

	ret = ti_sn65dsi86_write(dev, reg, buf[0]);
	if (ret)
		return ret;

	return ti_sn65dsi86_write(dev, reg + 1, buf[1]);
}

static int ti_sn65dsi86_update_bits(struct udevice *dev, u8 reg,
				    u8 mask, u8 val)
{
	u8 old_val;
	int ret;

	ret = ti_sn65dsi86_read(dev, reg, &old_val);
	if (ret) {
		dev_err(dev, "update_bits: failed to read reg 0x%02x, ret=%d\n", reg, ret);
		return ret;
	}

	u8 new_val = (old_val & ~mask) | (val & mask);
	dev_dbg(dev, "update_bits: reg 0x%02x: 0x%02x -> 0x%02x (mask=0x%02x, val=0x%02x)\n",
		reg, old_val, new_val, mask, val);

	ret = ti_sn65dsi86_write(dev, reg, new_val);
	if (ret) {
		dev_err(dev, "update_bits: failed to write reg 0x%02x with 0x%02x, ret=%d\n",
			reg, new_val, ret);
		return ret;
	}

	return 0;
}

static void ti_sn_bridge_set_refclk_freq(struct udevice *dev, struct ti_sn65dsi86_priv *priv,
					  struct display_timing *timing)
{
	int i;
	u32 refclk_rate_hz;
	const u32 *refclk_lut;
	size_t refclk_lut_size;
	struct clk refclk;
	int ret;

	/* Try to get refclk from device tree */
	ret = clk_get_by_name(dev, "refclk", &refclk);
	if (!ret) {
		/* External refclk pin is used */
		ret = clk_enable(&refclk);
		if (ret) {
			dev_warn(dev, "Failed to enable refclk: %d\n", ret);
		} else {
			refclk_rate_hz = clk_get_rate(&refclk);
			dev_dbg(dev, "Using external refclk at %u Hz\n", refclk_rate_hz);
			refclk_lut = ti_sn_bridge_refclk_lut;
			refclk_lut_size = ARRAY_SIZE(ti_sn_bridge_refclk_lut);

			/* Find matching frequency in LUT */
			for (i = 0; i < refclk_lut_size; i++)
				if (refclk_lut[i] == refclk_rate_hz)
					break;

			if (i >= refclk_lut_size) {
				/* Find closest frequency match instead of using default */
				u32 best_diff = UINT32_MAX;
				int best_idx = 1;  /* fallback */

				for (int j = 0; j < refclk_lut_size; j++) {
					u32 diff = (refclk_lut[j] > refclk_rate_hz) ?
						   (refclk_lut[j] - refclk_rate_hz) :
						   (refclk_rate_hz - refclk_lut[j]);
					if (diff < best_diff) {
						best_diff = diff;
						best_idx = j;
					}
				}
				i = best_idx;
				dev_warn(dev, "External refclk %u Hz not in LUT, using closest match index %d (%u Hz, diff=%u Hz)\n",
					 refclk_rate_hz, i, refclk_lut[i], best_diff);
			}

			dev_dbg(dev, "Setting external REFCLK_FREQ to index %d (%u Hz)\n", i, refclk_lut[i]);
			/* FORCE DSI clock as PLL source even if refclk is present to stay in sync with DPU */
			ti_sn65dsi86_update_bits(dev, SN_DPPLL_SRC_REG,
						 REFCLK_FREQ_MASK | DPPLL_CLK_SRC_DSICLK,
						 REFCLK_FREQ(i) | DPPLL_CLK_SRC_DSICLK);
			return;
		}
	}

	/* No refclk or failed to enable - use DSI clock as PLL source */
	dev_dbg(dev, "No external refclk available, using DSI clock as PLL source\n");

	/* Calculate DSI clock frequency: (pixel_clock * bpp) / (lanes * 2) */
	u32 bit_rate_khz = (timing->pixelclock.typ / 1000) * 24;
	u32 dsi_clk_khz = bit_rate_khz / (priv->dsi_lanes * 2);
	refclk_rate_hz = dsi_clk_khz * 1000;

	dev_dbg(dev, "Calculated DSI clock: %u Hz\n", refclk_rate_hz);

	refclk_lut = ti_sn_bridge_dsiclk_lut;
	refclk_lut_size = ARRAY_SIZE(ti_sn_bridge_dsiclk_lut);

	/* Find matching frequency in DSI clock LUT */
	for (i = 0; i < refclk_lut_size; i++)
		if (refclk_lut[i] == refclk_rate_hz)
			break;

	if (i >= refclk_lut_size) {
		/* Find closest frequency match instead of using default */
		u32 best_diff = UINT32_MAX;
		int best_idx = 1;  /* fallback */

		for (int j = 0; j < refclk_lut_size; j++) {
			u32 diff = (refclk_lut[j] > refclk_rate_hz) ?
				   (refclk_lut[j] - refclk_rate_hz) :
				   (refclk_rate_hz - refclk_lut[j]);
			if (diff < best_diff) {
				best_diff = diff;
				best_idx = j;
			}
		}
		i = best_idx;
		dev_warn(dev, "DSI clock %u Hz not in LUT, using closest match index %d (%u Hz, diff=%u Hz)\n",
			 refclk_rate_hz, i, refclk_lut[i], best_diff);
	}

	dev_dbg(dev, "Setting DSICLK mode, REFCLK_FREQ index %d (%u Hz)\n", i, refclk_lut[i]);
	/* Set DPPLL_CLK_SRC_DSICLK bit to use DSI clock */
	ti_sn65dsi86_update_bits(dev, SN_DPPLL_SRC_REG,
				 REFCLK_FREQ_MASK | DPPLL_CLK_SRC_DSICLK,
				 REFCLK_FREQ(i) | DPPLL_CLK_SRC_DSICLK);
}

static void ti_sn_bridge_set_dsi_rate(struct udevice *dev,
				      struct ti_sn65dsi86_priv *priv,
				      struct display_timing *timing)
{
	u32 raw_steps;
	u8 val;
	u8 min_steps = MIN_DSI_CLK_FREQ_MHZ / 5; /* 40MHz / 5MHz = 8 steps */
	u32 bpp = priv->bpp ? priv->bpp : 24;

	/*
	 * Calculate DSI clock based on: (pixel_clock * bpp) / (lanes * 2)
	 * SN_DSIA_CLK_FREQ register encoding: freq_MHz = value * 5
	 *
	 * Use DIV_ROUND_CLOSEST to avoid 1MHz mismatch (e.g. 445.5MHz -> 445MHz vs 444MHz)
	 */
	u64 bit_rate_hz = (u64)timing->pixelclock.typ * bpp;
	u64 clk_freq_hz = bit_rate_hz / (priv->dsi_lanes * 2);
	u32 clk_freq_mhz = DIV_ROUND_CLOSEST_ULL(clk_freq_hz, 1000000);

	dev_dbg(dev, "DSI clock calculation: PixelClock=%u, bpp=%u, lanes=%u -> %llu Hz (%u MHz)\n",
		 timing->pixelclock.typ, bpp, priv->dsi_lanes, clk_freq_hz, clk_freq_mhz);

	/* raw number of 5MHz steps */
	raw_steps = DIV_ROUND_CLOSEST(clk_freq_mhz, 5);

	/* Enforce minimum and follow kernel approach (min + ((raw-min) & 0xff)) */
	if (raw_steps < min_steps)
		val = min_steps;
	else
		val = min_steps + ((raw_steps - min_steps) & 0xFF);

	dev_dbg(dev, "Setting DSI clock divider to 0x%02x (%d MHz) for required %llu Hz (bpp=%u)\n",
		 val, val * 5, clk_freq_hz, bpp);

	ti_sn65dsi86_write(dev, SN_DSIA_CLK_FREQ_REG, val);
}

static int __maybe_unused ti_sn65dsi86_aux_write(struct udevice *dev, u32 address, u8 *data, int len)
{
	int ret;
	u8 val;
	u8 addr_buf[4];

	if (len > 16)
		return -EINVAL;

	/* Set address and length */
	/* SN_AUX_ADDR_19_16_REG (0x74) to SN_AUX_LENGTH_REG (0x77) */
	/* Address is 20 bits. Length is 8 bits. */
	/* Register layout:
	 * 0x74: ADDR[19:16]
	 * 0x75: ADDR[15:8]
	 * 0x76: ADDR[7:0]
	 * 0x77: LENGTH
	 */
	addr_buf[0] = (address >> 16) & 0x0F;
	addr_buf[1] = (address >> 8) & 0xFF;
	addr_buf[2] = address & 0xFF;
	addr_buf[3] = len;

	/* Write address and length */
	ret = dm_i2c_write(dev, SN_AUX_ADDR_19_16_REG, addr_buf, 4);
	if (ret)
		return ret;

	/* Write data to WDATA registers */
	if (len > 0) {
		ret = dm_i2c_write(dev, SN_AUX_WDATA_REG(0), data, len);
		if (ret)
			return ret;
	}

	/* Clear status bits */
	ti_sn65dsi86_write(dev, SN_AUX_CMD_STATUS_REG,
			   AUX_IRQ_STATUS_NAT_I2C_FAIL |
			   AUX_IRQ_STATUS_AUX_RPLY_TOUT |
			   AUX_IRQ_STATUS_AUX_SHORT);

	/* Send command (Native Write = 0x80) */
	/* AUX_CMD_REQ(0x8) | AUX_CMD_SEND */
	/* DP_AUX_NATIVE_WRITE is 0x8 */
	ti_sn65dsi86_write(dev, SN_AUX_CMD_REG, AUX_CMD_REQ(0x8) | AUX_CMD_SEND);

	/* Wait for completion */
	unsigned long start = get_timer(0);
	while (1) {
		ret = ti_sn65dsi86_read(dev, SN_AUX_CMD_REG, &val);
		if (ret)
			return ret;

		if (!(val & AUX_CMD_SEND))
			break;

		if (get_timer(start) > 50) /* 50ms timeout */
			return -ETIMEDOUT;
		udelay(100);
	}

	/* Check status */
	ret = ti_sn65dsi86_read(dev, SN_AUX_CMD_STATUS_REG, &val);
	if (ret)
		return ret;

	if (val & AUX_IRQ_STATUS_AUX_RPLY_TOUT) {
		dev_err(dev, "AUX write timeout\n");
		return -ETIMEDOUT;
	}

	if (val & AUX_IRQ_STATUS_NAT_I2C_FAIL) {
		dev_err(dev, "AUX write NACK\n");
		return -EIO;
	}

	return 0;
}

static int ti_sn65dsi86_aux_i2c_write(struct udevice *dev, u32 address, u8 *data, int len)
{
	int ret;
	u8 val;
	u8 addr_buf[4];

	if (len > 16)
		return -EINVAL;

	/* Set address and length */
	addr_buf[0] = (address >> 16) & 0x0F;
	addr_buf[1] = (address >> 8) & 0xFF;
	addr_buf[2] = address & 0xFF;
	addr_buf[3] = len;

	ret = dm_i2c_write(dev, SN_AUX_ADDR_19_16_REG, addr_buf, 4);
	if (ret)
		return ret;

	/* Write data to WDATA registers */
	if (len > 0) {
		ret = dm_i2c_write(dev, SN_AUX_WDATA_REG(0), data, len);
		if (ret)
			return ret;
	}

	/* Clear status bits */
	ti_sn65dsi86_write(dev, SN_AUX_CMD_STATUS_REG,
			   AUX_IRQ_STATUS_NAT_I2C_FAIL |
			   AUX_IRQ_STATUS_AUX_RPLY_TOUT |
			   AUX_IRQ_STATUS_AUX_SHORT);

	/* Send command (I2C Write = 0x4) */
	ti_sn65dsi86_write(dev, SN_AUX_CMD_REG, AUX_CMD_REQ(0x4) | AUX_CMD_SEND);

	/* Wait for completion */
	unsigned long start = get_timer(0);
	while (1) {
		ret = ti_sn65dsi86_read(dev, SN_AUX_CMD_REG, &val);
		if (ret)
			return ret;

		if (!(val & AUX_CMD_SEND))
			break;

		if (get_timer(start) > 50)
			return -ETIMEDOUT;
		udelay(100);
	}

	/* Check status */
	ret = ti_sn65dsi86_read(dev, SN_AUX_CMD_STATUS_REG, &val);
	if (ret)
		return ret;

	if (val & (AUX_IRQ_STATUS_NAT_I2C_FAIL | AUX_IRQ_STATUS_AUX_RPLY_TOUT | AUX_IRQ_STATUS_AUX_SHORT)) {
		dev_err(dev, "AUX I2C write failed: status=0x%02x\n", val);
		return -EIO;
	}

	return 0;
}

static int ti_sn65dsi86_aux_i2c_read(struct udevice *dev, u32 address, u8 *data, int len)
{
	int ret;
	u8 val;
	u8 addr_buf[4];

	if (len > 16)
		return -EINVAL;

	/* Set address and length */
	addr_buf[0] = (address >> 16) & 0x0F;
	addr_buf[1] = (address >> 8) & 0xFF;
	addr_buf[2] = address & 0xFF;
	addr_buf[3] = len;

	ret = dm_i2c_write(dev, SN_AUX_ADDR_19_16_REG, addr_buf, 4);
	if (ret)
		return ret;

	/* Clear status bits */
	ti_sn65dsi86_write(dev, SN_AUX_CMD_STATUS_REG,
			   AUX_IRQ_STATUS_NAT_I2C_FAIL |
			   AUX_IRQ_STATUS_AUX_RPLY_TOUT |
			   AUX_IRQ_STATUS_AUX_SHORT);

	/* Send command (I2C Read = 0x1) */
	/* 0x9 is Native Read, 0x1 is I2C Read */
	ti_sn65dsi86_write(dev, SN_AUX_CMD_REG, AUX_CMD_REQ(0x1) | AUX_CMD_SEND);

	/* Wait for completion */
	unsigned long start = get_timer(0);
	while (1) {
		ret = ti_sn65dsi86_read(dev, SN_AUX_CMD_REG, &val);
		if (ret)
			return ret;

		if (!(val & AUX_CMD_SEND))
			break;

		if (get_timer(start) > 50)
			return -ETIMEDOUT;
		udelay(100);
	}

	/* Check status */
	ret = ti_sn65dsi86_read(dev, SN_AUX_CMD_STATUS_REG, &val);
	if (ret)
		return ret;

	if (val & (AUX_IRQ_STATUS_NAT_I2C_FAIL | AUX_IRQ_STATUS_AUX_RPLY_TOUT | AUX_IRQ_STATUS_AUX_SHORT)) {
		dev_err(dev, "AUX I2C read failed: status=0x%02x\n", val);
		return -EIO;
	}

	/* Read data from RDATA registers */
	if (len > 0) {
		ret = dm_i2c_read(dev, SN_AUX_RDATA_REG(0), data, len);
		if (ret)
			return ret;
	}

	return 0;
}

static int ti_sn65dsi86_read_edid(struct udevice *dev, u8 *edid_buf)
{
	int i, ret;
	u8 offset;

	dev_dbg(dev, "Reading EDID...\n");

	for (i = 0; i < EDID_SIZE; i += 16) {
		/* Write offset to I2C address 0x50 */
		offset = i;
		ret = ti_sn65dsi86_aux_i2c_write(dev, 0x50, &offset, 1);
		if (ret) {
			dev_err(dev, "Failed to write EDID offset %d\n", i);
			return ret;
		}

		/* Read 16 bytes from I2C address 0x50 */
		ret = ti_sn65dsi86_aux_i2c_read(dev, 0x50, edid_buf + i, 16);
		if (ret) {
			dev_err(dev, "Failed to read EDID chunk at %d\n", i);
			return ret;
		}
	}

	dev_dbg(dev, "EDID read successful\n");
	return 0;
}

static int ti_sn_link_training(struct udevice *dev, int dp_rate_idx)
{
	u8 val;
	int ret;
	int i;

	/* Disable PLL before changing data rate */
	dev_dbg(dev, "Disabling PLL before rate change\n");
	ret = ti_sn65dsi86_write(dev, SN_PLL_ENABLE_REG, 0);
	if (ret) {
		dev_err(dev, "Failed to disable PLL\n");
		return ret;
	}
	mdelay(1); /* Allow PLL to fully disable */

	/* Set DP clock frequency value */
	dev_dbg(dev, "Setting DP data rate to index %d (%d kHz)\n",
		 dp_rate_idx, ti_sn_bridge_dp_rate_lut[dp_rate_idx]);
	ret = ti_sn65dsi86_update_bits(dev, SN_DATARATE_CONFIG_REG,
				       DP_DATARATE_MASK, DP_DATARATE(dp_rate_idx));
	if (ret) {
		dev_err(dev, "Failed to set DP data rate\n");
		return ret;
	}

	/* Enable DP PLL */
	ret = ti_sn65dsi86_write(dev, SN_PLL_ENABLE_REG, 1);
	if (ret)
		return ret;

	/* Wait for PLL lock */
	dev_dbg(dev, "Waiting for DP PLL lock...\n");
	for (i = 0; i < 100; i++) {
		ret = ti_sn65dsi86_read(dev, SN_DPPLL_SRC_REG, &val);
		if (ret)
			return ret;
		if (i == 0 || i == 10 || i == 50) {
			dev_dbg(dev, "PLL lock check %d: DPPLL_SRC_REG=0x%02x (bit7=lock)\n", i, val);
		}

		if (val & DPPLL_SRC_DP_PLL_LOCK)
			break;

		udelay(1000);
	}

	if (!(val & DPPLL_SRC_DP_PLL_LOCK)) {
		dev_err(dev, "DP PLL failed to lock (reg=0x%02x)\n", val);
		return -ETIMEDOUT;
	}
	dev_dbg(dev, "DP PLL locked successfully for rate index %d\n", dp_rate_idx);

	/* Check if DSI is active before attempting link training */
	ret = ti_sn65dsi86_read(dev, SN_DSI_LANES_REG, &val);
	if (!ret)
		dev_dbg(dev, "DSI_LANES_REG = 0x%02x\n", val);

	ret = ti_sn65dsi86_read(dev, SN_ENH_FRAME_REG, &val);
	if (!ret)
		dev_dbg(dev, "ENH_FRAME_REG = 0x%02x (VSTREAM_ENABLE = %s)\n",
			 val, (val & VSTREAM_ENABLE) ? "YES" : "NO");

	/* Attempt link training */
	for (i = 0; i < SN_LINK_TRAINING_TRIES; i++) {
		/* Semi auto link training mode */
		/* 0x0A = BIT(3) | BIT(1) ? No, 0x0A is 1010b.
		 * Bit 1 is SEMI_AUTO_LINK_TRAINING.
		 * Bit 3 is not documented in my snippet but likely related.
		 * Linux uses 0x0A.
		 */
		ret = ti_sn65dsi86_write(dev, SN_ML_TX_MODE_REG, 0x0A);
		if (ret)
			return ret;

		/* Wait for training to complete (up to 500ms) */
		unsigned long start = get_timer(0);
		while (1) {
			ret = ti_sn65dsi86_read(dev, SN_ML_TX_MODE_REG, &val);
			if (ret)
				return ret;

			if (val == ML_TX_MAIN_LINK_OFF || val == ML_TX_NORMAL_MODE)
				break;

			if (get_timer(start) > 500) {
				dev_err(dev, "Link training timed out (val=0x%02x)\n", val);
				return -ETIMEDOUT;
			}
			udelay(1000);
		}

		dev_dbg(dev, "ML_TX_MODE = 0x%02x (attempt %d)\n", val, i + 1);

		if (val == ML_TX_NORMAL_MODE) {
			dev_dbg(dev, "Link training succeeded on attempt %d\n", i + 1);
			return 0;
		}

		/* If we are here, val == ML_TX_MAIN_LINK_OFF */
		dev_dbg(dev, "Link training failed, main link off (attempt %d)\n", i + 1);

		/* Read AUX status for debug */
		ti_sn65dsi86_read(dev, SN_AUX_CMD_STATUS_REG, &val);
		dev_dbg(dev, "AUX_CMD_STATUS = 0x%02x (bit3=TOUT:%d, bit5=SHORT:%d, bit6=I2C_FAIL:%d)\n",
			 val, !!(val & AUX_IRQ_STATUS_AUX_RPLY_TOUT),
			 !!(val & AUX_IRQ_STATUS_AUX_SHORT),
			 !!(val & AUX_IRQ_STATUS_NAT_I2C_FAIL));

		/*
		 * If training failed, we might need to adjust swing/pre-emphasis manually
		 * or just retry. The bridge's semi-auto mode should handle this,
		 * but maybe we need to reset something?
		 */
		mdelay(10);
	}

	dev_err(dev, "Link training failed after %d tries\n",
		SN_LINK_TRAINING_TRIES);

	return -EIO;
}

static int ti_sn65dsi86_set_backlight(struct udevice *dev, int percent)
{
	struct ti_sn65dsi86_priv *priv = dev_get_priv(dev);
	u8 val;
	int ret;

	dev_dbg(dev, "set_backlight called with percent=%d\n", percent);

	/* Check if already enabled */
	ret = ti_sn65dsi86_read(dev, SN_ENH_FRAME_REG, &val);
	if (!ret && (val & VSTREAM_ENABLE)) {
		dev_dbg(dev, "VSTREAM already enabled\n");
		return 0;
	}

	/* Enable VSTREAM - set lane polarity and FORCE ASSR OFF (Linux order) */
	dev_dbg(dev, "Setting DP lane polarity and disabling ASSR before VSTREAM: ln_polrs=0x%02x\n", priv->ln_polrs);
	ti_sn65dsi86_update_bits(dev, SN_ENH_FRAME_REG, LN_POLRS_MASK | ASSR_CONTROL | VSTREAM_ENABLE,
				 (priv->ln_polrs << LN_POLRS_OFFSET) | VSTREAM_ENABLE);
	dev_dbg(dev, "Enabled VSTREAM and forced ASSR off\n");

	return 0;
}

static void ti_sn_bridge_set_video_timings(struct udevice *dev,
					   struct display_timing *timing)
{
	u8 hsync_polarity = 0, vsync_polarity = 0;
	u32 hsync_len, vsync_len, hback_porch, vback_porch;
	u32 hfront_porch, vfront_porch;
	u32 hsync_start, hsync_end, hdisplay, htotal;
	u32 vsync_start, vsync_end, vdisplay, vtotal;

	/* Extract values from timing struct */
	hdisplay = timing->hactive.typ;
	hsync_len = timing->hsync_len.typ;
	hback_porch = timing->hback_porch.typ;
	hfront_porch = timing->hfront_porch.typ;
	htotal = hdisplay + hfront_porch + hback_porch + hsync_len;
	hsync_start = hdisplay + hfront_porch;
	hsync_end = hsync_start + hsync_len;

	vdisplay = timing->vactive.typ;
	vsync_len = timing->vsync_len.typ;
	vback_porch = timing->vback_porch.typ;
	vfront_porch = timing->vfront_porch.typ;
	vtotal = vdisplay + vfront_porch + vback_porch + vsync_len;
	vsync_start = vdisplay + vfront_porch;
	vsync_end = vsync_start + vsync_len;

	/* Set polarity based on flags: 1 = Active Low, 0 = Active High */
	if (timing->flags & DISPLAY_FLAGS_HSYNC_LOW) {
		hsync_polarity = CHA_HSYNC_POLARITY;
		dev_dbg(dev, "HSYNC Polarity: Active LOW\n");
	} else {
		dev_dbg(dev, "HSYNC Polarity: Active HIGH\n");
	}

	if (timing->flags & DISPLAY_FLAGS_VSYNC_LOW) {
		vsync_polarity = CHA_VSYNC_POLARITY;
		dev_dbg(dev, "VSYNC Polarity: Active LOW\n");
	} else {
		dev_dbg(dev, "VSYNC Polarity: Active HIGH\n");
	}

	/* Program video timings (match Linux driver logic) */
	ti_sn65dsi86_write_u16(dev, SN_CHA_ACTIVE_LINE_LENGTH_LOW_REG, hdisplay);
	ti_sn65dsi86_write_u16(dev, SN_CHA_VERTICAL_DISPLAY_SIZE_LOW_REG, vdisplay);

	ti_sn65dsi86_write(dev, SN_CHA_HSYNC_PULSE_WIDTH_LOW_REG,
			(hsync_end - hsync_start) & 0xFF);
	ti_sn65dsi86_write(dev, SN_CHA_HSYNC_PULSE_WIDTH_HIGH_REG,
			(((hsync_end - hsync_start) >> 8) & 0x7F) | hsync_polarity);

	ti_sn65dsi86_write(dev, SN_CHA_VSYNC_PULSE_WIDTH_LOW_REG,
			(vsync_end - vsync_start) & 0xFF);
	ti_sn65dsi86_write(dev, SN_CHA_VSYNC_PULSE_WIDTH_HIGH_REG,
			(((vsync_end - vsync_start) >> 8) & 0x7F) | vsync_polarity);

	ti_sn65dsi86_write(dev, SN_CHA_HORIZONTAL_BACK_PORCH_REG,
			(htotal - hsync_end) & 0xFF);
	ti_sn65dsi86_write(dev, SN_CHA_VERTICAL_BACK_PORCH_REG,
			(vtotal - vsync_end) & 0xFF);

	ti_sn65dsi86_write(dev, SN_CHA_HORIZONTAL_FRONT_PORCH_REG,
			(hsync_start - hdisplay) & 0xFF);
	ti_sn65dsi86_write(dev, SN_CHA_VERTICAL_FRONT_PORCH_REG,
			(vsync_start - vdisplay) & 0xFF);

	/* Delay recommended by spec */
	mdelay(10);
}

static int ti_sn65dsi86_attach(struct udevice *dev)
{
	struct ti_sn65dsi86_priv *priv = dev_get_priv(dev);
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
	struct display_timing timing;
	u8 val;
	int ret;

	ret = mipi_dsi_attach(device);
	if (ret < 0)
		return ret;

	dev_dbg(dev, "%s\n", __func__);

	/* Wait for HPD with timeout */
	int hpd_wait = 0;
	for (hpd_wait = 0; hpd_wait < 20; hpd_wait++) {
		ret = ti_sn65dsi86_read(dev, SN_HPD_DISABLE_REG, &val);
		if (!ret && (val & HPD_DEBOUNCED_STATE)) {
			dev_dbg(dev, "HPD is HIGH (monitor detected after %d ms)\n", hpd_wait * 50);
			break;
		}

		mdelay(50);
	}

	/* Try to read EDID and use common parser */
	u8 edid_buf[EDID_SIZE];
	int bpp;
	if (ti_sn65dsi86_read_edid(dev, edid_buf) == 0) {
		if (edid_get_timing(edid_buf, EDID_SIZE, &timing, &bpp) == 0) {
			dev_dbg(dev, "EDID Timing: %dx%d pclk=%d\n",
			timing.hactive.typ, timing.vactive.typ, timing.pixelclock.typ);
			memcpy(&priv->timing, &timing, sizeof(timing));
			priv->bpp = (bpp <= 10) ? bpp * 3 : bpp;
			dev_dbg(dev, "Using %d bpp for DSI rate calc\n", priv->bpp);
			/* also debug the flags */
			dev_dbg(dev, "EDID Timing Flags: 0x%08x\n", timing.flags);
		} else {
			dev_err(dev, "Failed to parse EDID timing - using default\n");
			memcpy(&timing, &default_timing, sizeof(timing));
			priv->bpp = 24;
		}
	} else {
		dev_err(dev, "Failed to read EDID - using default timing\n");
		memcpy(&timing, &default_timing, sizeof(timing));
		priv->bpp = 24;
	}

	/* Configure DSI lanes - register value is (4 - actual_dsi_lanes) */
	val = CHA_DSI_LANES(4 - priv->dsi_lanes);
	dev_dbg(dev, "DSI lane config: dsi_lanes=%d, DP_lanes=%d, CHA_DSI_LANES value=0x%02x\n",
		priv->dsi_lanes, priv->dp_lanes, val);

	/* Set lane assignment register (Linux does this) */
	dev_dbg(dev, "Writing LN_ASSIGN_REG: 0x%02x\n", priv->ln_assign);
	ti_sn65dsi86_write(dev, SN_LN_ASSIGN_REG, priv->ln_assign);

	ret = ti_sn65dsi86_update_bits(dev, SN_DSI_LANES_REG,
				 CHA_DSI_LANES_MASK, val);
	if (ret) {
		dev_err(dev, "Failed to write DSI_LANES_REG: %d\n", ret);
		return ret;
	}

	/* Set DSI clock rate */
	ti_sn_bridge_set_dsi_rate(dev, priv, &timing);

	/* Configure reference clock (needs timing for DSI clock fallback) */
	ti_sn_bridge_set_refclk_freq(dev, priv, &timing);

	/* For DP: disable scrambling (Linux does this for full DP, not eDP) */
	dev_dbg(dev, "DisplayPort mode: Disabling scrambling (Linux behavior)\n");
	ti_sn65dsi86_update_bits(dev, SN_TRAINING_SETTING_REG,
				 SCRAMBLE_DISABLE, SCRAMBLE_DISABLE);

	/* Set Data Format to 24bpp */
	ti_sn65dsi86_update_bits(dev, SN_DATA_FORMAT_REG,
				 BPP_18_RGB, 0);

	/* Configure DP lanes */
	val = priv->dp_lanes;
	if (val > 3)
		val = 3;

	ti_sn65dsi86_update_bits(dev, SN_SSC_CONFIG_REG,
				 DP_NUM_LANES_MASK,
				 DP_NUM_LANES(val));

	/* Start Link Training */
	dev_dbg(dev, "Starting link training...\n");
	int dp_rate_idx;
	/* Start from index 4 (2.7 Gbps) to ensure enough bandwidth and better monitor compatibility */
	for (dp_rate_idx = 4; dp_rate_idx < ARRAY_SIZE(ti_sn_bridge_dp_rate_lut); dp_rate_idx++) {
		dev_dbg(dev, "Attempting link training at rate index %d (%d kHz)\n",
			 dp_rate_idx, ti_sn_bridge_dp_rate_lut[dp_rate_idx]);

		ret = ti_sn_link_training(dev, dp_rate_idx);
		if (ret == 0) {
			dev_dbg(dev, "Link training SUCCESS at rate %d kHz!\n",
				 ti_sn_bridge_dp_rate_lut[dp_rate_idx]);
			goto enable_vstream;
		}
	}

	/* Fallback to lower rates if 2.7G+ failed */
	for (dp_rate_idx = 1; dp_rate_idx < 4; dp_rate_idx++) {
		dev_dbg(dev, "Fallback: Attempting link training at rate index %d (%d kHz)\n",
			 dp_rate_idx, ti_sn_bridge_dp_rate_lut[dp_rate_idx]);

		ret = ti_sn_link_training(dev, dp_rate_idx);
		if (ret == 0) {
			dev_dbg(dev, "Link training SUCCESS at rate %d kHz!\n",
				 ti_sn_bridge_dp_rate_lut[dp_rate_idx]);
			goto enable_vstream;
		}
	}

	dev_err(dev, "Failed to train link at any data rate\n");
	return -EIO;

enable_vstream:
	dev_dbg(dev, "Link training complete\n");

	/* Ensure monitor is in D0 (on) power state via DPCD */
	u8 power_state = 0x01; /* DP_SET_POWER_D0 */
	ret = ti_sn65dsi86_aux_write(dev, 0x600, &power_state, 1);
	if (ret) {
		dev_warn(dev, "Failed to set monitor power state D0: %d\n", ret);
	} else {
		dev_dbg(dev, "Set monitor to D0 power state via DPCD\n");
		mdelay(10);
	}

	/* Configure video timings AFTER link training (Linux order!) */
	dev_dbg(dev, "Configuring video timings after link training\n");
	ti_sn_bridge_set_video_timings(dev, &timing);

	/* CRITICAL: Do NOT enable VSTREAM early - let DPU FrameGen sync first */
	dev_dbg(dev, "NOT enabling VSTREAM early - waiting for DPU FrameGen to sync\n");

	/* Set lane polarity and DISABLE ASSR (for standard DP monitors) */
	dev_dbg(dev, "Setting DP lane polarity: ln_polrs=0x%02x, disabling ASSR\n", priv->ln_polrs);
	ti_sn65dsi86_update_bits(dev, SN_ENH_FRAME_REG, LN_POLRS_MASK | ASSR_CONTROL | VSTREAM_ENABLE,
				 (priv->ln_polrs << LN_POLRS_OFFSET));
	mdelay(1);

	ret = ti_sn65dsi86_set_backlight(dev, 100);
	if (ret) {
		dev_err(dev, "Backlight was not enabled properly: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ti_sn65dsi86_probe(struct udevice *dev)
{
	struct ti_sn65dsi86_priv *priv = dev_get_priv(dev);
	int ret;
	u8 val;

	dev_dbg(dev, "%s\n", __func__);

	priv->dev = dev;

	/* Match Linux sequence: I2C/regmap is already done by DM, now get GPIO */
	ret = gpio_request_by_name(dev, "enable-gpios", 0, &priv->enable,
				   GPIOD_IS_OUT);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "Failed to get enable GPIO: %d\n", ret);
		return ret;
	}
	if (ret == -ENOENT)
		dev_warn(dev, "No enable-gpios found\n");
	else
		dev_dbg(dev, "%s: enable GPIO acquired successfully\n", __func__);

	/* Now get regulators */
	ret = device_get_supply_regulator(dev, "vccio-supply", &priv->vccio_reg);
	if (ret && ret != -ENOENT)
		dev_warn(dev, "Failed to get vccio regulator: %d\n", ret);

	ret = device_get_supply_regulator(dev, "vpll-supply", &priv->vpll_reg);
	if (ret && ret != -ENOENT)
		dev_warn(dev, "Failed to get vpll regulator: %d\n", ret);

	ret = device_get_supply_regulator(dev, "vcca-supply", &priv->vcca_reg);
	if (ret && ret != -ENOENT)
		dev_warn(dev, "Failed to get vcca regulator: %d\n", ret);

	ret = device_get_supply_regulator(dev, "vcc-supply", &priv->vcc_reg);
	if (ret && ret != -ENOENT)
		dev_warn(dev, "Failed to get vcc regulator: %d\n", ret);

	/* Power up the bridge in probe() like lt8912 does */
	if (priv->vccio_reg) regulator_set_enable(priv->vccio_reg, true);
	if (priv->vpll_reg) regulator_set_enable(priv->vpll_reg, true);
	if (priv->vcca_reg) regulator_set_enable(priv->vcca_reg, true);
	if (priv->vcc_reg) regulator_set_enable(priv->vcc_reg, true);

	if (dm_gpio_is_valid(&priv->enable)) {
		dev_dbg(dev, "Setting enable GPIO HIGH in probe()\n");
		dm_gpio_set_value(&priv->enable, 1);
	}

	/* Default DSI settings */
	priv->dsi_lanes = 4;
	priv->lanes = 4;
	priv->format = MIPI_DSI_FMT_RGB888;
	priv->mode_flags = MIPI_DSI_MODE_VIDEO;
	priv->bpp = 24;

	/* Parse data-lanes from port@0 (DSI input) - like lt8912 */
	ofnode ports, port, endpoint;
	ports = dev_read_subnode(dev, "ports");
	if (ofnode_valid(ports)) {
		port = ofnode_find_subnode(ports, "port@0");
		if (ofnode_valid(port)) {
			endpoint = ofnode_find_subnode(port, "endpoint");
			if (ofnode_valid(endpoint)) {
				int len = ofnode_read_size(endpoint, "data-lanes");
				if (len > 0) {
					priv->dsi_lanes = len / sizeof(u32);
					priv->lanes = priv->dsi_lanes;
					dev_dbg(dev, "Parsed port@0: dsi_lanes=%d\n", priv->dsi_lanes);
				}
			}
		}

		/* Parse data-lanes and lane-polarities from port@1 (DP output) */
		port = ofnode_find_subnode(ports, "port@1");
		if (ofnode_valid(port)) {
			endpoint = ofnode_find_subnode(port, "endpoint");

			if (ofnode_valid(endpoint)) {
				u32 lane_assignments[SN_MAX_DP_LANES] = { 0, 1, 2, 3 };
				u32 lane_polarities[SN_MAX_DP_LANES] = { 0, 0, 0, 0 };
				int ret_lanes = ofnode_read_u32_array(endpoint, "data-lanes", lane_assignments, SN_MAX_DP_LANES);
				if (!ret_lanes) {
					priv->dp_lanes = SN_MAX_DP_LANES;
					ofnode_read_u32_array(endpoint, "lane-polarities", lane_polarities, SN_MAX_DP_LANES);
				} else {
					for (int i = 3; i >= 1; i--) {
						ret_lanes = ofnode_read_u32_array(endpoint, "data-lanes", lane_assignments, i);
						if (!ret_lanes) {
							priv->dp_lanes = i;
							ofnode_read_u32_array(endpoint, "lane-polarities", lane_polarities, i);
							break;
						}
					}
				}

				priv->ln_assign = 0;
				priv->ln_polrs = 0;

				for (int i = SN_MAX_DP_LANES - 1; i >= 0; i--) {
					priv->ln_assign = priv->ln_assign << LN_ASSIGN_WIDTH | lane_assignments[i];
					priv->ln_polrs = priv->ln_polrs << 1 | lane_polarities[i];
				}

				dev_dbg(dev, "Parsed port@1: dp_lanes=%d, ln_assign=0x%02x, ln_polrs=0x%02x\n",
					 priv->dp_lanes, priv->ln_assign, priv->ln_polrs);
			}
		}
	}

	return 0;
}

static int ti_sn65dsi86_get_display_timing(struct udevice *dev,
					    struct display_timing *timings)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
	struct ti_sn65dsi86_priv *priv = dev_get_priv(dev);

	memcpy(timings, &default_timing, sizeof(*timings));

	/* i.MX95 DPU workaround: the Video PLL cannot reach 148.5MHz exactly.
	 * It reaches 145.45MHz. We MUST tell the DSI host and bridge the actual rate
	 * to avoid FIFO underflows, parity errors, and CHA_DSI_CLK_RANGE_ERR.
	 */
	if (timings->pixelclock.typ == 148500000) {
		timings->pixelclock.typ = 145454545;
		/* Force Active LOW polarities to match DPU default behavior and Lontium reference */
		timings->flags &= ~(DISPLAY_FLAGS_HSYNC_HIGH | DISPLAY_FLAGS_VSYNC_HIGH);
		timings->flags |= DISPLAY_FLAGS_HSYNC_LOW | DISPLAY_FLAGS_VSYNC_LOW;
	}

	/* CRITICAL: Set plat fields so DSI host can read them even if device is NULL */
	plat->lanes = priv->lanes;
	plat->format = priv->format;
	plat->mode_flags = priv->mode_flags;

	/* fill characteristics of DSI data link */
	if (device) {
		device->lanes = priv->lanes;
		device->format = priv->format;
		device->mode_flags = priv->mode_flags;
	}

	return 0;
}

static const struct panel_ops ti_sn65dsi86_ops = {
	.enable_backlight = ti_sn65dsi86_attach,
	.get_display_timing = ti_sn65dsi86_get_display_timing
};

static const struct udevice_id ti_sn65dsi86_ids[] = {
	{ .compatible = "ti,sn65dsi86" },
	{ }
};

U_BOOT_DRIVER(ti_sn65dsi86) = {
	.name = "ti_sn65dsi86",
	.id = UCLASS_PANEL,
	.of_match = ti_sn65dsi86_ids,
	.probe = ti_sn65dsi86_probe,
	.ops = &ti_sn65dsi86_ops,
	.priv_auto = sizeof(struct ti_sn65dsi86_priv),
	.plat_auto = sizeof(struct mipi_dsi_panel_plat),
};
