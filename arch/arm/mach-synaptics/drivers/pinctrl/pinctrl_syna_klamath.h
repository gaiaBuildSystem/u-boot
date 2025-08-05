/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2016~2023 Synaptics Incorporated. All rights reserved.
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

#ifndef __PINCTRL_SYNA_KLAMATH_H
#define __PINCTRL_SYNA_KLAMATH_H

#include <dm/device.h>
#include <dm/pinctrl.h>

#include "pinctrl_syna.h"

static const struct syna_desc_group klamath_soc_pinctrl_groups[] = {
	SYNA_PINCTRL_GROUP("GPIO23", 0x0, 0x3, 0x00,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO23 */
			SYNA_PINCTRL_FUNC(0x1, "tw2"), /* SCL */
			SYNA_PINCTRL_FUNC(0x2, "rgmii"), /* MDC */
			SYNA_PINCTRL_FUNC(0x3, "key_row0"),
			SYNA_PINCTRL_FUNC(0x5, "spi3"), /* SS2n */
			SYNA_PINCTRL_FUNC(0x6, "key_col3"),
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT4 */
	SYNA_PINCTRL_GROUP("GPIO24", 0x0, 0x3, 0x03,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO24 */
			SYNA_PINCTRL_FUNC(0x1, "tw2"), /* SDA */
			SYNA_PINCTRL_FUNC(0x2, "rgmii"), /* MDIO */
			SYNA_PINCTRL_FUNC(0x3, "key_row1"),
			SYNA_PINCTRL_FUNC(0x5, "spi3"), /* SS3n */
			SYNA_PINCTRL_FUNC(0x6, "key_col2"),
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT5 */
	SYNA_PINCTRL_GROUP("GPIO25", 0x0, 0x3, 0x06,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO25 */
			SYNA_PINCTRL_FUNC(0x1, "uart5"), /* RXD */
			SYNA_PINCTRL_FUNC(0x2, "gpio_trig1"),
			SYNA_PINCTRL_FUNC(0x3, "key_row2"),
			SYNA_PINCTRL_FUNC(0x4, "sm_uart1"), /* RXD */
			SYNA_PINCTRL_FUNC(0x6, "key_col1"),
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT6 */
	SYNA_PINCTRL_GROUP("GPIO26", 0x0, 0x3, 0x09,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO26 */
			SYNA_PINCTRL_FUNC(0x1, "uart5"), /* TXD */
			SYNA_PINCTRL_FUNC(0x2, "rgmii"), /* PTP_PPS_O */
			SYNA_PINCTRL_FUNC(0x4, "sm_uart1"), /* TXD */
			SYNA_PINCTRL_FUNC(0x5, "usb2"), /* VBUS */
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT7 */
	SYNA_PINCTRL_GROUP("GPIO27", 0x0, 0x3, 0x0c,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO27 */
			SYNA_PINCTRL_FUNC(0x1, "tw3"), /* SCL */
			SYNA_PINCTRL_FUNC(0x2, "uart4"), /* TXD */
			SYNA_PINCTRL_FUNC(0x4, "sm_uart1")), /* RTSn */
	SYNA_PINCTRL_GROUP("GPIO28", 0x0, 0x3, 0x0f,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO28 */
			SYNA_PINCTRL_FUNC(0x1, "tw3"), /* SDA */
			SYNA_PINCTRL_FUNC(0x2, "uart4"), /* RXD */
			SYNA_PINCTRL_FUNC(0x4, "sm_uart1")), /* CTSn */
	SYNA_PINCTRL_GROUP("GPIO29", 0x0, 0x3, 0x12,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO29 */
			SYNA_PINCTRL_FUNC(0x2, "uart4"), /* DE */
			SYNA_PINCTRL_FUNC(0x3, "key_row4"),
			SYNA_PINCTRL_FUNC(0x4, "sm_uart1"), /* RXD */
			SYNA_PINCTRL_FUNC(0x6, "key_col4"),
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO30", 0x0, 0x3, 0x15,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO30 */
			SYNA_PINCTRL_FUNC(0x2, "uart4"), /* REn */
			SYNA_PINCTRL_FUNC(0x3, "key_row5"),
			SYNA_PINCTRL_FUNC(0x4, "sm_uart1"), /* TXD */
			SYNA_PINCTRL_FUNC(0x6, "key_col5"),
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO31", 0x0, 0x3, 0x18,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO31 */
			SYNA_PINCTRL_FUNC(0x2, "rgmii"), /* MDC */
			SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SS3n */
			SYNA_PINCTRL_FUNC(0x6, "spi3"), /* SS2n */
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS2n */
	SYNA_PINCTRL_GROUP("GPIO32", 0x0, 0x3, 0x1b,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO32 */
			SYNA_PINCTRL_FUNC(0x2, "rgmii"), /* MDIO */
			SYNA_PINCTRL_FUNC(0x6, "spi3"), /* SS3n */
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS3n */
	SYNA_PINCTRL_GROUP("GPIO33", 0x4, 0x3, 0x00,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO33 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TD0 */
			SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* TXD0 */
			SYNA_PINCTRL_FUNC(0x5, "can0"), /* TX */
			SYNA_PINCTRL_FUNC(0x6, "spi3"), /* SS1n */
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS1n */
	SYNA_PINCTRL_GROUP("GPIO34", 0x4, 0x3, 0x03,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO34 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TD1 */
			SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* TXD1 */
			SYNA_PINCTRL_FUNC(0x5, "can0")), /* RX */
	SYNA_PINCTRL_GROUP("GPIO35", 0x4, 0x3, 0x06,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO35 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TD2 */
			SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* TXD0 */
			SYNA_PINCTRL_FUNC(0x3, "key_col7"),
			SYNA_PINCTRL_FUNC(0x4, "uart5"), /* RXD */
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS0n */
	SYNA_PINCTRL_GROUP("GPIO36", 0x4, 0x3, 0x09,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO36 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TD3 */
			SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* TXD1 */
			SYNA_PINCTRL_FUNC(0x4, "uart5"), /* TXD */
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SDO */
	SYNA_PINCTRL_GROUP("GPIO37", 0x4, 0x3, 0x0c,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO37 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RD0 */
			SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* RXD0 */
			SYNA_PINCTRL_FUNC(0x5, "can1"), /* TX */
			SYNA_PINCTRL_FUNC(0x7, "spi5")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO38", 0x4, 0x3, 0x0f,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO38 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RD1 */
			SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* RXD1 */
			SYNA_PINCTRL_FUNC(0x5, "can1"), /* RX */
			SYNA_PINCTRL_FUNC(0x7, "spi5")), /* SDO */
	SYNA_PINCTRL_GROUP("GPIO39", 0x4, 0x3, 0x12,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO39 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RD2 */
			SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* RXD0 */
			SYNA_PINCTRL_FUNC(0x3, "key_row6"),
			SYNA_PINCTRL_FUNC(0x4, "uart6"), /* RXD */
			SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SS1n*/
			SYNA_PINCTRL_FUNC(0x6, "spi4")), /* SS3n  */
	SYNA_PINCTRL_GROUP("GPIO40", 0x4, 0x3, 0x15,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO40 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RD3 */
			SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* RXD1 */
			SYNA_PINCTRL_FUNC(0x3, "key_row7"),
			SYNA_PINCTRL_FUNC(0x4, "uart6"), /* TXD */
			SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SS2n */
			SYNA_PINCTRL_FUNC(0x6, "spi4")), /* SS2n  */
	SYNA_PINCTRL_GROUP("GPIO41", 0x4, 0x3, 0x18,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO41 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RXC */
			SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* CRSDV */
			SYNA_PINCTRL_FUNC(0x7, "spi5")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO42", 0x4, 0x3, 0x1b,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO42 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TXC */
			SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* CRSDV */
			SYNA_PINCTRL_FUNC(0x3, "key_row8"),
			SYNA_PINCTRL_FUNC(0x4, "uart7"), /* RXD */
			SYNA_PINCTRL_FUNC(0x6, "spi4")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO43", 0x8, 0x3, 0x00,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO43 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TXCTL */
			SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* TXEN */
			SYNA_PINCTRL_FUNC(0x6, "spi4")), /* SS0n */
	SYNA_PINCTRL_GROUP("GPIO44", 0x8, 0x3, 0x03,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO44 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RXCTL */
			SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* TXEN */
			SYNA_PINCTRL_FUNC(0x3, "key_row9"),
			SYNA_PINCTRL_FUNC(0x4, "uart7"), /* TXD */
			SYNA_PINCTRL_FUNC(0x6, "spi4")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO45", 0x8, 0x3, 0x06,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO45 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* CLKOUT */
			SYNA_PINCTRL_FUNC(0x2, "rmii1")), /* REFCLK */
	SYNA_PINCTRL_GROUP("GPIO46", 0x8, 0x3, 0x09,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO46 */
			SYNA_PINCTRL_FUNC(0x1, "sdio1"), /* CDn */
			SYNA_PINCTRL_FUNC(0x2, "sdio2"), /* CDn */
			SYNA_PINCTRL_FUNC(0x3, "key_col0"),
			SYNA_PINCTRL_FUNC(0x5, "sm_uart1"), /* RTSn */
			SYNA_PINCTRL_FUNC(0x6, "key_row9"),
			SYNA_PINCTRL_FUNC(0x7, "dsi")), /* TE */
	SYNA_PINCTRL_GROUP("GPIO47", 0x8, 0x3, 0x0c,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO47 */
			SYNA_PINCTRL_FUNC(0x1, "sdio1"), /* WP */
			SYNA_PINCTRL_FUNC(0x2, "sdio2"), /* WP */
			SYNA_PINCTRL_FUNC(0x3, "key_col1"),
			SYNA_PINCTRL_FUNC(0x4, "rmii2"), /* REFCLK */
			SYNA_PINCTRL_FUNC(0x5, "sm_uart1"), /* CTSn */
			SYNA_PINCTRL_FUNC(0x6, "key_row8")),
	SYNA_PINCTRL_GROUP("GPIO48", 0x8, 0x3, 0x0f,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO48 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TD0 */
			SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SS0n */
			SYNA_PINCTRL_FUNC(0x6, "spi4"), /* SS2n */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS3n */
	SYNA_PINCTRL_GROUP("GPIO49", 0x8, 0x3, 0x12,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO49 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TD1 */
			SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SDO */
			SYNA_PINCTRL_FUNC(0x6, "spi4"), /* SS3n */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS2n */
	SYNA_PINCTRL_GROUP("GPIO50", 0x8, 0x3, 0x15,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO50 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TD2 */
			SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SCLK */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS1n */
	SYNA_PINCTRL_GROUP("GPIO51", 0x8, 0x3, 0x18,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO51 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TD3 */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS0n */
	SYNA_PINCTRL_GROUP("GPIO52", 0x8, 0x3, 0x1b,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO52 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RD0 */
			SYNA_PINCTRL_FUNC(0x6, "spi4"), /* SS2n */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SDO */
	SYNA_PINCTRL_GROUP("GPIO53", 0xc, 0x3, 0x00,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO53 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RD1 */
			SYNA_PINCTRL_FUNC(0x6, "spi4"), /* SS3n */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO54", 0xc, 0x3, 0x03,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO54 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RD2 */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO55", 0xc, 0x3, 0x06,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO55 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RD3 */
			SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SDI */
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS1n */
	SYNA_PINCTRL_GROUP("GPIO56", 0xc, 0x3, 0x09,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO56 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RXC */
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS0n */
	SYNA_PINCTRL_GROUP("GPIO57", 0xc, 0x3, 0x0c,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO57 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TXC */
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SDO */
	SYNA_PINCTRL_GROUP("GPIO58", 0xc, 0x3, 0x0f,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO58 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TXCTL */
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO59", 0xc, 0x3, 0x12,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO59 */
			SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RXCTL */
			SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO0", 0xc, 0x3, 0x15,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO0 */
			SYNA_PINCTRL_FUNC(0x1, "i2s1"), /* LRCK */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS0n */
	SYNA_PINCTRL_GROUP("GPIO1", 0xc, 0x3, 0x18,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO1 */
			SYNA_PINCTRL_FUNC(0x1, "i2s1"), /* BCLK */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO2", 0xc, 0x3, 0x1b,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO2*/
			SYNA_PINCTRL_FUNC(0x1, "i2s1"), /* DO */
			SYNA_PINCTRL_FUNC(0x4, "spdif"), /* O */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SDO */
	SYNA_PINCTRL_GROUP("GPIO3", 0x10, 0x3, 0x00,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO3 */
			SYNA_PINCTRL_FUNC(0x1, "i2s1"), /* MCLK */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS1n */
	SYNA_PINCTRL_GROUP("GPIO4", 0x10, 0x3, 0x03,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO4 */
			SYNA_PINCTRL_FUNC(0x1, "i2s1"), /* DI */
			SYNA_PINCTRL_FUNC(0x4, "spdif"), /* I */
			SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO5", 0x10, 0x3, 0x06,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO5 */
			SYNA_PINCTRL_FUNC(0x1, "i2s2"), /* LRCK */
			SYNA_PINCTRL_FUNC(0x2, "cam"), /* PIXCLK */
			SYNA_PINCTRL_FUNC(0x3, "key_row0"),
			SYNA_PINCTRL_FUNC(0x4, "spdif"), /* I */
			SYNA_PINCTRL_FUNC(0x6, "key_col7")),
	SYNA_PINCTRL_GROUP("GPIO6", 0x10, 0x3, 0x09,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO6 */
			SYNA_PINCTRL_FUNC(0x1, "i2s2"), /* BCLK */
			SYNA_PINCTRL_FUNC(0x2, "cam"), /* HSYNC */
			SYNA_PINCTRL_FUNC(0x3, "key_row1"),
			SYNA_PINCTRL_FUNC(0x4, "spdif"), /* O */
			SYNA_PINCTRL_FUNC(0x6, "key_col6")),
	SYNA_PINCTRL_GROUP("GPIO7", 0x10, 0x3, 0x0c,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO7 */
			SYNA_PINCTRL_FUNC(0x1, "i2s2"), /* DO */
			SYNA_PINCTRL_FUNC(0x3, "key_row2"),
			SYNA_PINCTRL_FUNC(0x4, "spdif"), /* O */
			SYNA_PINCTRL_FUNC(0x5, "sm_pdm"), /* CLKIO */
			SYNA_PINCTRL_FUNC(0x6, "key_col5")),
	SYNA_PINCTRL_GROUP("GPIO8", 0x10, 0x3, 0x0f,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO8 */
			SYNA_PINCTRL_FUNC(0x1, "i2s2"), /* DI */
			SYNA_PINCTRL_FUNC(0x2, "cam"), /* VSYNC */
			SYNA_PINCTRL_FUNC(0x3, "key_row3"),
			SYNA_PINCTRL_FUNC(0x4, "spdif"), /* I */
			SYNA_PINCTRL_FUNC(0x6, "key_col4")),
	SYNA_PINCTRL_GROUP("GPIO9", 0x10, 0x3, 0x12,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO9 */
			SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA0 */
			SYNA_PINCTRL_FUNC(0x4, "pdm")), /* DI1 */
	SYNA_PINCTRL_GROUP("GPIO10", 0x10, 0x3, 0x15,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO10 */
			SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA1 */
			SYNA_PINCTRL_FUNC(0x4, "pdm"), /* DI2 */
			SYNA_PINCTRL_FUNC(0x5, "dsi")), /* TE */
	SYNA_PINCTRL_GROUP("GPIO11", 0x10, 0x3, 0x18,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO11 */
			SYNA_PINCTRL_FUNC(0x1, "i2s2"), /* MCLK */
			SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA2 */
			SYNA_PINCTRL_FUNC(0x4, "sm_pdm"), /* CLKIO */
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* CLK */
	SYNA_PINCTRL_GROUP("GPIO12", 0x10, 0x3, 0x1b,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO12 */
			SYNA_PINCTRL_FUNC(0x1, "i2s3"), /* LRCK */
			SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA3 */
			SYNA_PINCTRL_FUNC(0x3, "sdio2"), /* WP */
			SYNA_PINCTRL_FUNC(0x4, "sdio1"), /* WP */
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT0 */
	SYNA_PINCTRL_GROUP("GPIO13", 0x14, 0x3, 0x00,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO13 */
			SYNA_PINCTRL_FUNC(0x1, "i2s3"), /* BCLK */
			SYNA_PINCTRL_FUNC(0x2, "pdm"), /* DI1 */
			SYNA_PINCTRL_FUNC(0x3, "rgmii"), /* PTP_PPS_O */
			SYNA_PINCTRL_FUNC(0x4, "usb2"), /* VBUS */
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT1 */
	SYNA_PINCTRL_GROUP("GPIO14", 0x14, 0x3, 0x03,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO14 */
			SYNA_PINCTRL_FUNC(0x1, "i2s3"), /* DO */
			SYNA_PINCTRL_FUNC(0x4, "pdm"), /* DI3 */
			SYNA_PINCTRL_FUNC(0x6, "spdif"), /* O */
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT2 */
	SYNA_PINCTRL_GROUP("GPIO15", 0x14, 0x3, 0x06,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO15 */
			SYNA_PINCTRL_FUNC(0x1, "i2s3"), /* DI */
			SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA4 */
			SYNA_PINCTRL_FUNC(0x4, "sm_pdm"), /* DI0 */
			SYNA_PINCTRL_FUNC(0x6, "spdif"), /* I */
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT3 */
	SYNA_PINCTRL_GROUP("GPIO16", 0x14, 0x3, 0x09,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO16 */
			SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SS0n */
			SYNA_PINCTRL_FUNC(0x3, "sdio2")), /* DAT3 */
	SYNA_PINCTRL_GROUP("GPIO17", 0x14, 0x3, 0x0c,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO17 */
			SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SS1n */
			SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA5 */
			SYNA_PINCTRL_FUNC(0x3, "sdio2"), /* DAT2 */
			SYNA_PINCTRL_FUNC(0x5, "dsi")), /* TE */
	SYNA_PINCTRL_GROUP("GPIO18", 0x14, 0x3, 0x0f,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO18 */
			SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SS2n */
			SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA6 */
			SYNA_PINCTRL_FUNC(0x3, "sdio2"), /* DAT1 */
			SYNA_PINCTRL_FUNC(0x4, "pdm"), /* DI2 */
			SYNA_PINCTRL_FUNC(0x5, "can1")), /* RX */
	SYNA_PINCTRL_GROUP("GPIO19", 0x14, 0x3, 0x12,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO19 */
			SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SS3n */
			SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA7 */
			SYNA_PINCTRL_FUNC(0x3, "sdio2"), /* DAT0 */
			SYNA_PINCTRL_FUNC(0x4, "pdm"), /* DI3 */
			SYNA_PINCTRL_FUNC(0x5, "can1")), /* TX */
	SYNA_PINCTRL_GROUP("GPIO20", 0x14, 0x3, 0x15,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO20 */
			SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SDO */
			SYNA_PINCTRL_FUNC(0x3, "sdio2")), /* CMD */
	SYNA_PINCTRL_GROUP("GPIO21", 0x14, 0x3, 0x18,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO21 */
			SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SCLK */
			SYNA_PINCTRL_FUNC(0x2, "sdio2"), /* CLK */
			SYNA_PINCTRL_FUNC(0x5, "clkout")),
	SYNA_PINCTRL_GROUP("GPIO22", 0x14, 0x3, 0x1b,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO22 */
			SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SDI */
			SYNA_PINCTRL_FUNC(0x3, "sdio2"), /* CDn */
			SYNA_PINCTRL_FUNC(0x4, "sdio1")), /* CDn */
};

static const struct syna_desc_group klamath_sysmgr_pinctrl_groups[] = {
	SYNA_PINCTRL_GROUP("SM_GPIO31", 0x0, 0x3, 0x00,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO31*/
			SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM0*/
			SYNA_PINCTRL_FUNC(0x2, "uart1"), /* SM UART1 RXD */
			SYNA_PINCTRL_FUNC(0x3, "key_row7"),
			SYNA_PINCTRL_FUNC(0x4, "pdm"), /* SM PDM DI0 */
			SYNA_PINCTRL_FUNC(0x5, "uart0"), /* SM UART0 RXD */
			SYNA_PINCTRL_FUNC(0x6, "can1")), /* SM CAN1 RX */
	SYNA_PINCTRL_GROUP("SM_GPIO32", 0x0, 0x3, 0x03,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO32 */
			SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM1 */
			SYNA_PINCTRL_FUNC(0x2, "uart1"), /* SM UART1 TXD */
			SYNA_PINCTRL_FUNC(0x3, "key_col0"),
			SYNA_PINCTRL_FUNC(0x4, "pdm"), /* SM PDM CLKIO */
			SYNA_PINCTRL_FUNC(0x5, "uart0"), /* SM UART0 TXD */
			SYNA_PINCTRL_FUNC(0x6, "can1")), /* SM CAN1 TX */
	SYNA_PINCTRL_GROUP("SM_GPIO33", 0x0, 0x3, 0x06,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO33 */
			SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM2 */
			SYNA_PINCTRL_FUNC(0x3, "uart2"), /* SM UART2 TXD */
			SYNA_PINCTRL_FUNC(0x4, "uart3"), /* SM UART3 RTSn */
			SYNA_PINCTRL_FUNC(0x5, "key_row5"),
			SYNA_PINCTRL_FUNC(0x6, "uart3_de")), /* SM UART3 DE */
	SYNA_PINCTRL_GROUP("SM_GPIO34", 0x0, 0x3, 0x09,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO34 */
			SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM3 */
			SYNA_PINCTRL_FUNC(0x3, "uart2"), /* SM UART2 RXD */
			SYNA_PINCTRL_FUNC(0x4, "uart3"), /* SM UART3 CTSn */
			SYNA_PINCTRL_FUNC(0x5, "key_row4"),
			SYNA_PINCTRL_FUNC(0x6, "uart3_ren")), /* SM UART3 REn */
	SYNA_PINCTRL_GROUP("SM_GPIO35", 0x0, 0x3, 0x0c,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO35 */
			SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM4 */
			SYNA_PINCTRL_FUNC(0x2, "uart1"), /* SM UART1 RTSn */
			SYNA_PINCTRL_FUNC(0x3, "uart3"), /* SM UART3 TXD */
			SYNA_PINCTRL_FUNC(0x4, "uart2"), /* SM UART2 RTSn */
			SYNA_PINCTRL_FUNC(0x5, "key_row3"),
			SYNA_PINCTRL_FUNC(0x6, "uart0")), /* SM UART0 RTSn */
	SYNA_PINCTRL_GROUP("SM_GPIO36", 0x0, 0x3, 0x0f,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO36 */
			SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM5 */
			SYNA_PINCTRL_FUNC(0x2, "uart1"), /* SM UART1 CTSn */
			SYNA_PINCTRL_FUNC(0x3, "uart3"), /* SM UART3 RXD */
			SYNA_PINCTRL_FUNC(0x4, "uart2"), /* SM UART2 CTSn */
			SYNA_PINCTRL_FUNC(0x5, "key_row2"),
			SYNA_PINCTRL_FUNC(0x6, "uart0")), /* SM UART0 CTSn */
	SYNA_PINCTRL_GROUP("SM_GPIO37", 0x0, 0x3, 0x12,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO37 */
			SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM6 */
			SYNA_PINCTRL_FUNC(0x3, "tw0"), /* SM TW0 SCL */
			SYNA_PINCTRL_FUNC(0x4, "key_row6"),
			SYNA_PINCTRL_FUNC(0x5, "pdm")), /* SM PDM CLKIO */
	SYNA_PINCTRL_GROUP("SM_GPIO38", 0x0, 0x3, 0x15,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO38 */
			SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM7 */
			SYNA_PINCTRL_FUNC(0x3, "tw0"), /* SM TW0 SDA */
			SYNA_PINCTRL_FUNC(0x5, "pdm")), /* SM PDM DI0 */
	SYNA_PINCTRL_GROUP("SM_GPIO3", 0x0, 0x3, 0x18,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO3 */
			SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SS0n */
			SYNA_PINCTRL_FUNC(0x2, "tw1"), /* SM TW1 SCL */
			SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM8 */
			SYNA_PINCTRL_FUNC(0x4, "i3c")), /* SM I3C MS SCL */
	SYNA_PINCTRL_GROUP("SM_GPIO4", 0x0, 0x3, 0x1b,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO4 */
			SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SS1n */
			SYNA_PINCTRL_FUNC(0x2, "tw1"), /* SM TW1 SDA */
			SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM0 */
			SYNA_PINCTRL_FUNC(0x4, "i3c")), /* SM I3C MS SDA */
	SYNA_PINCTRL_GROUP("SM_GPIO5", 0x4, 0x3, 0x00,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO5 */
			SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SS2n */
			SYNA_PINCTRL_FUNC(0x3, "pwm")), /* SM PWM1 */
	SYNA_PINCTRL_GROUP("SM_GPIO6", 0x4, 0x3, 0x03,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO6 */
			SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SS3n */
			SYNA_PINCTRL_FUNC(0x2, "spi1s"), /* SM SPI1S SSn */
			SYNA_PINCTRL_FUNC(0x3, "pwm")), /* SM PWM2 */
	SYNA_PINCTRL_GROUP("SM_GPIO9", 0x4, 0x3, 0x06,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO9 */
			SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SDO */
			SYNA_PINCTRL_FUNC(0x2, "spi1s"), /* SM SPI1S SDO */
			SYNA_PINCTRL_FUNC(0x3, "pwm")), /* SM PWM3 */
	SYNA_PINCTRL_GROUP("SM_GPIO10", 0x4, 0x3, 0x09,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO10 */
			SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SCLK */
			SYNA_PINCTRL_FUNC(0x2, "spi1s"), /* SM SPI1S SCLK */
			SYNA_PINCTRL_FUNC(0x3, "pwm")), /* SM PWM4 */
	SYNA_PINCTRL_GROUP("SM_GPIO11", 0x4, 0x3, 0x0c,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO11 */
			SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SDI */
			SYNA_PINCTRL_FUNC(0x2, "spi1s"), /* SM SPI1S SDI */
			SYNA_PINCTRL_FUNC(0x3, "pwm")), /* SM PWM5 */
	SYNA_PINCTRL_GROUP("SM_GPIO30", 0x4, 0x3, 0x0f,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO30 */
			SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI DATA7 */
			SYNA_PINCTRL_FUNC(0x3, "key_col6"),
			SYNA_PINCTRL_FUNC(0x7, "sm_clkout")),
	SYNA_PINCTRL_GROUP("SM_GPIO29", 0x4, 0x3, 0x12,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO29 */
			SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI DATA6 */
			SYNA_PINCTRL_FUNC(0x2, "uart3"), /* SM UART3 RXD */
			SYNA_PINCTRL_FUNC(0x3, "key_col5"),
			SYNA_PINCTRL_FUNC(0x4, "uart2"), /* SM UART2 CTSn */
			SYNA_PINCTRL_FUNC(0x5, "uart0"), /* SM UART0 CTSn */
			SYNA_PINCTRL_FUNC(0x6, "key_row2"),
			SYNA_PINCTRL_FUNC(0x7, "uart1")), /* SM UART1 CTSn */
	SYNA_PINCTRL_GROUP("SM_GPIO28", 0x4, 0x3, 0x15,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO28 */
			SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI DATA5 */
			SYNA_PINCTRL_FUNC(0x2, "uart3"), /* SM UART3 TXD */
			SYNA_PINCTRL_FUNC(0x3, "key_col4"),
			SYNA_PINCTRL_FUNC(0x4, "uart2"), /* SM UART2 RTSn */
			SYNA_PINCTRL_FUNC(0x5, "uart0"), /* SM UART0 RTSn */
			SYNA_PINCTRL_FUNC(0x6, "key_row3"),
			SYNA_PINCTRL_FUNC(0x7, "uart1")), /* SM UART1 RTSn */
	SYNA_PINCTRL_GROUP("SM_GPIO27", 0x4, 0x3, 0x18,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO27 */
			SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI DATA4 */
			SYNA_PINCTRL_FUNC(0x2, "uart2"), /* SM UART2 RXD */
			SYNA_PINCTRL_FUNC(0x3, "key_col3"),
			SYNA_PINCTRL_FUNC(0x4, "uart3"), /* SM UART3 CTSn */
			SYNA_PINCTRL_FUNC(0x5, "uart3_ren"), /* SM UART3 REn */
			SYNA_PINCTRL_FUNC(0x6, "key_row4")),
	SYNA_PINCTRL_GROUP("SM_GPIO26", 0x4, 0x3, 0x1b,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO26 */
			SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI CS1n */
			SYNA_PINCTRL_FUNC(0x2, "uart2"), /* SM UART2 TXD */
			SYNA_PINCTRL_FUNC(0x3, "key_col2"),
			SYNA_PINCTRL_FUNC(0x4, "uart3"), /* SM UART3 RTSn */
			SYNA_PINCTRL_FUNC(0x5, "uart3_de"), /* SM UART3 DE */
			SYNA_PINCTRL_FUNC(0x6, "key_row5"),
			SYNA_PINCTRL_FUNC(0x7, "sm_clkout")),
	SYNA_PINCTRL_GROUP("SM_GPIO25", 0x8, 0x3, 0x00,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO25 */
			SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI DQS */
			SYNA_PINCTRL_FUNC(0x6, "key_row6")),
	SYNA_PINCTRL_GROUP("SM_GPIO24", 0x8, 0x3, 0x03,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO24 */
			SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI CLKn */
	SYNA_PINCTRL_GROUP("SM_GPIO23", 0x8, 0x3, 0x06,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO23 */
			SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI CLK */
	SYNA_PINCTRL_GROUP("SM_GPIO22", 0x8, 0x3, 0x09,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO22 */
			SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI DATA3 */
	SYNA_PINCTRL_GROUP("SM_GPIO21", 0x8, 0x3, 0x0c,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO21 */
			SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI DATA2 */
	SYNA_PINCTRL_GROUP("SM_GPIO20", 0x8, 0x3, 0x0f,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO20 */
			SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI DATA1 */
	SYNA_PINCTRL_GROUP("SM_GPIO19", 0x8, 0x3, 0x12,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO19 */
			SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI DATA0 */
	SYNA_PINCTRL_GROUP("SM_GPIO18", 0x8, 0x3, 0x15,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO18 */
			SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI CS0n*/
	SYNA_PINCTRL_GROUP("SM_GPIO17", 0x8, 0x3, 0x18,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO17 */
			SYNA_PINCTRL_FUNC(0x1, "uart1"), /* SM UART1 TXD */
			SYNA_PINCTRL_FUNC(0x2, "can0"), /* SM CAN0 TX */
			SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM8 */
			SYNA_PINCTRL_FUNC(0x6, "uart0")), /* SM UART0 TXD */
	SYNA_PINCTRL_GROUP("SM_GPIO15", 0x8, 0x3, 0x1b,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO15 */
			SYNA_PINCTRL_FUNC(0x1, "tw1"), /* SM TW1 SDA */
			SYNA_PINCTRL_FUNC(0x2, "uart0"), /* SM UART0 RTSn */
			SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM11 */
			SYNA_PINCTRL_FUNC(0x4, "can0"), /* SM CAN0 TX */
			SYNA_PINCTRL_FUNC(0x6, "uart1"), /* SM UART1 RTSn */
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* DBG OUT */
	SYNA_PINCTRL_GROUP("SM_GPIO14", 0xc, 0x3, 0x00,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO14 */
			SYNA_PINCTRL_FUNC(0x1, "tw1"), /* SM TW1 SCL */
			SYNA_PINCTRL_FUNC(0x2, "uart0"), /* SM UART0 CTSn */
			SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM10 */
			SYNA_PINCTRL_FUNC(0x4, "can0"), /* SM CAN0 RX */
			SYNA_PINCTRL_FUNC(0x6, "uart1"), /* SM UART1 CTSn */
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* DBG OUT */
	SYNA_PINCTRL_GROUP("SM_GPIO16", 0xc, 0x3, 0x03,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO16 */
			SYNA_PINCTRL_FUNC(0x1, "uart1"), /* SM UART1 RXD */
			SYNA_PINCTRL_FUNC(0x2, "can0"), /* SM CAN0 RX */
			SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM7 */
			SYNA_PINCTRL_FUNC(0x6, "uart1")), /* SM UART0 RXD */
	SYNA_PINCTRL_GROUP("SM_GPIO13", 0xc, 0x3, 0x06,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO13 */
			SYNA_PINCTRL_FUNC(0x1, "tw0"), /* SM TW0 SDA */
			SYNA_PINCTRL_FUNC(0x2, "i3c"), /* SM I3C MS SDA */
			SYNA_PINCTRL_FUNC(0x3, "sm_clkout"),
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* DBG out */
	SYNA_PINCTRL_GROUP("SM_GPIO12", 0xc, 0x3, 0x09,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO12 */
			SYNA_PINCTRL_FUNC(0x1, "tw0"), /* SM TW0 SCL */
			SYNA_PINCTRL_FUNC(0x2, "i3c"), /* SM I3C MS SCL */
			SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM6 */
			SYNA_PINCTRL_FUNC(0x7, "dbg")), /* DBG CLK */
	SYNA_PINCTRL_GROUP("SM_GPIO8", 0xc, 0x3, 0x0c,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO8 */
			SYNA_PINCTRL_FUNC(0x1, "uart0"), /* SM UART0 TXD */
			SYNA_PINCTRL_FUNC(0x2, "can0"), /* SM CAN0 TX */
			SYNA_PINCTRL_FUNC(0x3, "sm_clkout"),
			SYNA_PINCTRL_FUNC(0x6, "uart1")), /* SM UART0 TXD */
	SYNA_PINCTRL_GROUP("SM_GPIO7", 0xc, 0x3, 0x0f,
			SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO7 */
			SYNA_PINCTRL_FUNC(0x1, "uart0"), /* SM UART0 RXD */
			SYNA_PINCTRL_FUNC(0x2, "can0"), /* SM CAN0 RX */
			SYNA_PINCTRL_FUNC(0x3, "key_row6"),
			SYNA_PINCTRL_FUNC(0x4, "gpio_trig"), /* TRIG2 */
			SYNA_PINCTRL_FUNC(0x5, "pwm"), /* SM PWM9 */
			SYNA_PINCTRL_FUNC(0x6, "uart1")), /* SM UART0 RXD */
	SYNA_PINCTRL_GROUP("SM_GPIO2", 0xc, 0x3, 0x12,
			SYNA_PINCTRL_FUNC(0x0, "jtag"), /* TDO */
			SYNA_PINCTRL_FUNC(0x1, "gpio"), /* SM GPIO2 */
			SYNA_PINCTRL_FUNC(0x2, "pdm"), /* SM PDM CLKIO */
			SYNA_PINCTRL_FUNC(0x3, "i2s2"), /* MCLK */
			SYNA_PINCTRL_FUNC(0x6, "pwm")), /* SM PWM11 */
	SYNA_PINCTRL_GROUP("SM_GPIO1", 0xc, 0x3, 0x15,
			SYNA_PINCTRL_FUNC(0x0, "jtag"), /* TDI */
			SYNA_PINCTRL_FUNC(0x1, "gpio"), /* SM GPIO1 */
			SYNA_PINCTRL_FUNC(0x2, "uart0"), /* SM UART0 RXD */
			SYNA_PINCTRL_FUNC(0x3, "key_col1"),
			SYNA_PINCTRL_FUNC(0x4, "gpio_trig"), /* TRIG0 */
			SYNA_PINCTRL_FUNC(0x5, "pwm"), /* SM PWM9 */
			SYNA_PINCTRL_FUNC(0x6, "key_row7")),
	SYNA_PINCTRL_GROUP("SM_GPIO0", 0xc, 0x3, 0x18,
			SYNA_PINCTRL_FUNC(0x0, "jtag"), /* TMS */
			SYNA_PINCTRL_FUNC(0x1, "gpio"), /* SM GPIO0 */
			SYNA_PINCTRL_FUNC(0x2, "uart0"), /* SM UART0 TXD */
			SYNA_PINCTRL_FUNC(0x3, "key_col0"),
			SYNA_PINCTRL_FUNC(0x5, "pdm"), /* SM PDM DI0 */
			SYNA_PINCTRL_FUNC(0x6, "pwm")), /* SM PWM10 */
};

static const struct syna_pinctrl_desc klamath_soc_pinctrl_data = {
	.groups = klamath_soc_pinctrl_groups,
	.ngroups = ARRAY_SIZE(klamath_soc_pinctrl_groups),
};

static const struct syna_pinctrl_desc klamath_sysmgr_pinctrl_data = {
	.groups = klamath_sysmgr_pinctrl_groups,
	.ngroups = ARRAY_SIZE(klamath_sysmgr_pinctrl_groups),
};

#endif /* __PINCTRL_SYNA_KLAMATH_H */
