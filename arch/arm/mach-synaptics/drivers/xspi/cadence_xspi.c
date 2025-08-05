// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016~2025 Synaptics Incorporated. All rights reserved.
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

#include <clk.h>
#include <log.h>
#include <dm.h>
#include <fdtdec.h>
#include <malloc.h>
#include <reset.h>
#include <spi.h>
#include <spi-mem.h>
#include <dm/device_compat.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/sizes.h>
#include <linux/delay.h>
#include <linux/time.h>
#include "cadence_xspi.h"

static void cdns_xspi_reset(struct cdns_xspi_priv *priv)
{
	reset_assert_bulk(&priv->resets);
	mdelay(1);
	reset_deassert_bulk(&priv->resets);
}

static int cdns_xspi_wait_for_controller_idle(struct cdns_xspi_priv *priv)
{
	u32 ctrl_stat;
	u32 timeout = 10;

	do {
		ctrl_stat = readl(priv->regbase + CDNS_XSPI_CTRL_STATUS_REG);
		if ((ctrl_stat & CDNS_XSPI_CTRL_BUSY) == 0)
			return 0;

		mdelay(1);
	} while (--timeout);

	return -ETIMEDOUT;
}

static int cdns_xspi_wait_for_controller_init_completed(struct cdns_xspi_priv *priv)
{
	u32 ctrl_stat;
	u32 timeout = 10;

	do {
		ctrl_stat = readl(priv->regbase + CDNS_XSPI_CTRL_STATUS_REG);
		if (ctrl_stat & CDNS_XSPI_INIT_COMPLETED)
			return 0;

		mdelay(1);
	} while (--timeout);

	return -ETIMEDOUT;
}

static int cdns_xspi_configure_clock(struct cdns_xspi_priv *priv)
{
#ifdef CONFIG_SYS_BOARD_FPGA
	u32  rdata;
	u32 divider = 3;

	rdata = readl(priv->regbase + 0x1008); //special for FPGA XSPI_CLOCK_MODE_SETTINGS
	rdata &= ~(0xF << 24);
	rdata |= ((divider & 0xF) << 24);
	writel(rdata, priv->regbase + 0x1008);
#else
	debug("%s,%d\n", __func__, __LINE__);
#endif

	return 0;
}

static int cdns_xspi_set_data_mode(struct cdns_xspi_priv *priv)
{
	int ret = 0;
	int mode = CDNS_XSPI_WORK_MODE_STIG;

	ret = cdns_xspi_wait_for_controller_idle(priv);
	if (ret < 0)
		return -EIO;

	writel(FIELD_PREP(CDNS_XSPI_CTRL_WORK_MODE, CDNS_XSPI_WORK_MODE_STIG),
	       priv->regbase + CDNS_XSPI_CTRL_CONFIG_REG);
}

static bool cdns_xspi_is_dll_locked(struct cdns_xspi_priv *priv)
{
	u32 dll_lock;
	u32 timeout = 10;

	do {
		dll_lock = readl(priv->regbase + CDNS_XSPI_INTR_STATUS_REG);
		if ((dll_lock & CDNS_XSPI_DLL_LOCK) == 1)
			return 0;

		mdelay(1);
	} while (--timeout);

	return -ETIMEDOUT;
}

static int cdns_xspi_configure_phy(struct cdns_xspi_priv *priv)
{
	u32  rdata = 0;
	u32 rd_dly = 3;

	rdata = readl(priv->regbase + CDNS_XSPI_DLL_PHY_CTRL);
	writel(rdata & (~CDNS_XSPI_DLL_RST_N), priv->regbase + CDNS_XSPI_DLL_PHY_CTRL);

	writel(0x80000101, priv->regbase + CDNS_XSPI_PHY_DATASLICE_RFILE_PHY_DQ_TIMING);
	writel(0x00000404 | (INTERNAL_LPBK_DQS << LPBK_DQS_SET_POS), priv->regbase + CDNS_XSPI_PHY_DATASLICE_RFILE_PHY_DQS_TIMING);
	writel(0x00000030 | (rd_dly << RD_DEL_SEL_POS), priv->regbase + CDNS_XSPI_PHY_DATASLICE_RFILE_PHY_GATE_LPBK_CTRL);
	writel(0x00000013, priv->regbase + CDNS_XSPI_PHY_DATASLICE_RFILE_PHY_DLL_MASTER_CTRL);
	writel(0x00000f3f, priv->regbase + CDNS_XSPI_PHY_DATASLICE_RFILE_PHY_DLL_SLAVE_CTRL);
	writel(0x0, priv->regbase + CDNS_XSPI_PHY_CTB_RFILE_PHY_CTRL);

	rdata = readl(priv->regbase + CDNS_XSPI_DLL_PHY_CTRL);
	writel(rdata | CDNS_XSPI_DLL_RST_N, priv->regbase + CDNS_XSPI_DLL_PHY_CTRL);

	return cdns_xspi_is_dll_locked(priv);
}

int cdns_xspi_init(struct cdns_xspi_priv *priv)
{
	cdns_xspi_configure_clock(priv);
	cdns_xspi_reset(priv);
	writel(0, 0xE5025A00);//RA_mcu_gbl_cfg_XSPI_SRAM_PWR

	cdns_xspi_wait_for_controller_init_completed(priv);

	cdns_xspi_configure_phy(priv);

	cdns_xspi_set_data_mode(priv);
}

static void cdns_xspi_trigger_command(struct cdns_xspi_priv *priv,
				      u32 cmd_regs[5])
{
	writel(cmd_regs[4], priv->regbase + CDNS_XSPI_CMD_REG_4);
	writel(cmd_regs[3], priv->regbase + CDNS_XSPI_CMD_REG_3);
	writel(cmd_regs[2], priv->regbase + CDNS_XSPI_CMD_REG_2);
	writel(cmd_regs[1], priv->regbase + CDNS_XSPI_CMD_REG_1);
	writel(cmd_regs[0], priv->regbase + CDNS_XSPI_CMD_REG_0);
}

static int cdns_xspi_probe(struct udevice *bus)
{
	struct cdns_xspi_plat *plat = dev_get_plat(bus);
	struct cdns_xspi_priv *priv = dev_get_priv(bus);

	priv->regbase = plat->regbase;
	priv->membase = plat->membase;
	priv->memsize = plat->memsize;
	priv->resets = plat->resets;

	cdns_xspi_init(priv);

	return 0;
}

static int cdns_xspi_remove(struct udevice *dev)
{
	struct cdns_xspi_priv *priv = dev_get_priv(dev);
	int ret = 0;

	debug("%s,%d\n", __func__, __LINE__);

	return ret;
}

static int cdns_xspi_set_speed(struct udevice *bus, uint hz)
{
	debug("%s,%d bus %s, hz %d\n", __func__, __LINE__, bus->name, hz);

	return 0;
}

static int cdns_xspi_set_mode(struct udevice *bus, uint mode)
{
	struct cdns_xspi_priv *priv = dev_get_priv(bus);

	debug("%s,%d mode %d\n", __func__, __LINE__, mode);

	return 0;
}

static void cdns_xspi_wait_stig_completion(struct cdns_xspi_priv *priv)
{
	u32 irq_status;
	u32 cmd_status;
	u32 timeout = 10;
	int ret = 0;

	do {
		irq_status = readl(priv->regbase + CDNS_XSPI_INTR_STATUS_REG);
	} while (!(irq_status & CDNS_XSPI_STIG_DONE));

	writel(CDNS_XSPI_STIG_DONE, priv->regbase + CDNS_XSPI_INTR_STATUS_REG);

	do {
		cmd_status = readl(priv->regbase + CDNS_XSPI_CMD_STATUS_REG);
		if (cmd_status)
			break;
		else
			mdelay(1);
	} while (--timeout);

	if (cmd_status & CDNS_XSPI_CMD_STATUS_COMPLETED) {
		if ((cmd_status & CDNS_XSPI_CMD_STATUS_FAILED) != 0) {
			if (cmd_status & CDNS_XSPI_CMD_STATUS_DQS_ERROR) {
				puts("Incorrect DQS pulses detected\n");
				ret = -EPROTO;
			}
			if (cmd_status & CDNS_XSPI_CMD_STATUS_CRC_ERROR) {
				puts("CRC error received\n");
				ret = -EPROTO;
			}
			if (cmd_status & CDNS_XSPI_CMD_STATUS_BUS_ERROR) {
				puts("Error resp on system DMA interface\n");
				ret = -EPROTO;
			}
			if (cmd_status & CDNS_XSPI_CMD_STATUS_INV_SEQ_ERROR) {
				puts("Invalid command sequence detected\n");
				ret = -EPROTO;
			}
		}
	} else {
		puts("Fatal err - command not completed\n");
		ret = -EPROTO;
	}

	return ret;
}

static void cdns_xspi_wait_sdma_completion(struct cdns_xspi_priv *priv)
{
	u32 irq_status;

	do {
		irq_status = readl(priv->regbase + CDNS_XSPI_INTR_STATUS_REG);
	} while (!(irq_status & CDNS_XSPI_SDMA_TRIGGER));

	writel(CDNS_XSPI_SDMA_TRIGGER, priv->regbase + CDNS_XSPI_INTR_STATUS_REG);
}

static cdns_xspi_ioread8_rep(void         *addr, void *buffer, uint32_t count)
{
	u32 *buf32 = (uint32_t *)buffer;
	u32 aligned_count = count / sizeof(uint32_t);
	u32 remainder = count % sizeof(uint32_t);
	u8 *buf8 = (uint8_t *)buffer + (aligned_count * sizeof(uint32_t));

	while (aligned_count--)
		*buf32++ = readl(addr);

	while (remainder--)
		*buf8++ = readb(addr);
}

void cdns_xspi_iowrite8_rep(void *addr, const void *buffer, uint32_t count)
{
	const u32 *buf32 = (const uint32_t *)buffer;
	u32 aligned_count = count / sizeof(uint32_t);
	u32 remainder = count % sizeof(uint32_t);
	const u8 *buf8 = (const u8 *)buffer + (aligned_count * sizeof(uint32_t));

	while (aligned_count--)
		writel(*buf32++, addr);

	while (remainder--)
		writeb(*buf8++, addr);
}

static void cdns_xspi_sdma_handle(struct cdns_xspi_priv *priv)
{
	u32 sdma_size, sdma_trd_info;
	u8 sdma_dir;

	sdma_size = readl(priv->regbase + CDNS_XSPI_SDMA_SIZE_REG);
	sdma_trd_info = readl(priv->regbase + CDNS_XSPI_SDMA_TRD_INFO_REG);
	sdma_dir = FIELD_GET(CDNS_XSPI_SDMA_DIR, sdma_trd_info);

	switch (sdma_dir) {
	case CDNS_XSPI_SDMA_DIR_READ:
		cdns_xspi_ioread8_rep(priv->membase, priv->in_buffer, sdma_size);
		break;

	case CDNS_XSPI_SDMA_DIR_WRITE:
		cdns_xspi_iowrite8_rep(priv->membase, priv->out_buffer, sdma_size);
		break;
	}
}

static int cdns_xspi_mem_exec_op(struct spi_slave *spi,
#ifdef CONFIG_SYS_BOARD_FPGA
				   struct spi_mem_op *op)
#else
				   const struct spi_mem_op *op)
#endif
{
	struct udevice *bus = spi->dev->parent;
	struct cdns_xspi_priv *priv = dev_get_priv(bus);

	u32 rdata = 0;

	unsigned long stick_count = 1;
	double perf = 0;
	u32 cmd_regs[5];
	u32 data_phase = op->data.dir != SPI_MEM_NO_DATA;
	int ret = 0;

	if (priv->cur_cs != spi_chip_select(spi->dev))
		priv->cur_cs = spi_chip_select(spi->dev);

#ifdef CONFIG_SYS_BOARD_FPGA
	op->addr.buswidth = 1;
	if (op->cmd.opcode == SPINOR_OP_RDID) {
		op->data.nbytes = 0xc;
	}
#endif

	ret = cdns_xspi_wait_for_controller_idle(priv);
	if (ret < 0)
		return -EIO;

	memset(cmd_regs, 0, sizeof(cmd_regs));
	cmd_regs[1] = CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_1(op, data_phase);
	cmd_regs[2] = CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_2(op);
	cmd_regs[3] = CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_3(op);
	cmd_regs[4] = CDNS_XSPI_CMD_FLD_P1_INSTR_CMD_4(op, priv->cur_cs);

	cdns_xspi_trigger_command(priv, cmd_regs);

	if (data_phase)	{
		cmd_regs[0] = CDNS_XSPI_STIG_DONE_FLAG;
		cmd_regs[1] = CDNS_XSPI_CMD_FLD_DSEQ_CMD_1(op);
		cmd_regs[2] = CDNS_XSPI_CMD_FLD_DSEQ_CMD_2(op);
		cmd_regs[3] = CDNS_XSPI_CMD_FLD_DSEQ_CMD_3(op);
		cmd_regs[4] = CDNS_XSPI_CMD_FLD_DSEQ_CMD_4(op, priv->cur_cs);

		priv->in_buffer = op->data.buf.in;
		priv->out_buffer = op->data.buf.out;

		cdns_xspi_trigger_command(priv, cmd_regs);

		cdns_xspi_wait_sdma_completion(priv);
		cdns_xspi_sdma_handle(priv);
	}

	cdns_xspi_wait_stig_completion(priv);

	return ret;
}

static int cdns_xspi_of_to_plat(struct udevice *bus)
{
	struct cdns_xspi_plat *plat = dev_get_plat(bus);
	struct cdns_xspi_priv *priv = dev_get_priv(bus);
	int ret;

	/* Get the controller base address */
	plat->regbase = devfdt_get_addr_index_ptr(bus, 0);
	if (!plat->regbase)
		return -EINVAL;

	/* Get the config space base address and size */
	plat->membase = devfdt_get_addr_size_index_ptr(bus, 1,
						       &plat->memsize);

	ret = reset_get_bulk(bus, &plat->resets);
	if (ret) {
		dev_warn(bus, "Can't get reset: %d\n", ret);
		if (ret == -ENOENT || ret == -EOPNOTSUPP)
			return 0;
		else
			return ret;
	}

	if (!plat->membase)
		return -EINVAL;

	debug("%s: regbase=%p membase=%p\n",
	       __func__, plat->regbase, plat->membase);

	return 0;
}

static const struct spi_controller_mem_ops cdns_xspi_mem_ops = {
	.exec_op = cdns_xspi_mem_exec_op,
};

static const struct dm_spi_ops cdns_xspi_ops = {
	.set_speed	= cdns_xspi_set_speed,
	.set_mode	= cdns_xspi_set_mode,
	.mem_ops	= &cdns_xspi_mem_ops,
	/*
	 * cs_info is not needed, since we require all chip selects to be
	 * in the device tree explicitly
	 */
};

static const struct udevice_id cdns_xspi_ids[] = {
	{ .compatible = "cdns,xspi-nor" },
	{ }
};

U_BOOT_DRIVER(cdns_xspi) = {
	.name = "cadence_xspi",
	.id = UCLASS_SPI,
	.of_match = cdns_xspi_ids,
	.ops = &cdns_xspi_ops,
	.of_to_plat = cdns_xspi_of_to_plat,
	.plat_auto	= sizeof(struct cdns_xspi_plat),
	.priv_auto	= sizeof(struct cdns_xspi_priv),
	.probe = cdns_xspi_probe,
	.remove = cdns_xspi_remove,
	.flags = DM_FLAG_OS_PREPARE,
};
