// SPDX-License-Identifier: GPL-2.0+
/*
 * Qualcomm QCS8300 (Monaco) display under the EFI app: DSI0 + ADV7535 HDMI
 *
 * UEFI on this platform provides no GOP and leaves the display hardware
 * powered down, so this drives it directly: RPMh votes, display clocks,
 * DSI0 PHY/PLL/controller, the DPU (DMA0 -> LM0 -> PP0 -> INTF1), an SMMU
 * identity mapping and the ADV7535 bridge over bit-banged i2c12. The
 * sequences follow the Linux msm, dispcc, adv7511 and rpmh drivers.
 *
 * The video driver in qcom_mdss_video.c uses mdss_display_start(). The
 * mdssprobe command runs the same code step by step, printing as it goes:
 *
 *   mdssprobe info  - read-only: GCC clock state, RSC config, cmd-db entries
 *   mdssprobe power - vote MMCX via RPMh, enable display clocks / GDSC and
 *                     read the MDSS, DPU and DSI hardware versions
 *   mdssprobe bw    - vote the MMNOC bus (BCM MM0/MM1) so DISP_HF_AXI runs
 *   mdssprobe hdmi [dsitpg] - stage 1: 720p60 over DSI0 + ADV7535 with the
 *                     INTF border/underflow colours (or the DSI test pattern)
 *   mdssprobe hdmi status - DSI, INTF and ADV7535 status
 *   mdssprobe fb    - stage 3: colour bars fetched by the DMA0 pipe from a
 *                     framebuffer through the SMMU
 *   mdssprobe i2c   - read the ADV7535 IDs over bit-banged gpio45/46
 *   mdssprobe i2c scan - list the devices answering on i2c12
 *   mdssprobe i2c geni - same as 'i2c' but using the QUP1 SE4 engine
 *   mdssprobe smmu  - dump the apps SMMU stream mapping for the MDSS SID
 *   mdssprobe smmu map - add an identity (S1 bypass) mapping for the MDSS
 *
 * Each step is printed before it is done, so a hang points at its cause.
 */

#include <command.h>
#include <cpu_func.h>
#include <efi.h>
#include <efi_api.h>
#include <log.h>
#include <time.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <soc/qcom/geni-se.h>
#include "qcom_mdss.h"

/* GCC */
#define GCC_DISP_AHB_CBCR	0x00133004
#define GCC_DISP_HF_AXI_CBCR	0x00133010
#define GCC_DISP_XO_CBCR	0x00133018
#define GCC_QUP1_M_AHB_CBCR	0x00124004
#define GCC_QUP1_S_AHB_CBCR	0x00124008
#define GCC_QUP1_CORE_CBCR	0x0012400c
#define GCC_QUP1_CORE2X_CBCR	0x00124018
#define GCC_QUP1_S4_CBCR	0x0012461c
#define GCC_QUP1_S4_CMD_RCGR	0x00124624
#define GCC_APCS_VOTE		0x0014b008
#define GCC_VOTE_QUP1_CORE2X	BIT(18)
#define GCC_VOTE_QUP1_CORE	BIT(19)
#define GCC_VOTE_QUP1_M_AHB	BIT(20)
#define GCC_VOTE_QUP1_S_AHB	BIT(21)
#define GCC_VOTE_QUP1_S4	BIT(26)

/* Branch clock control */
#define CBCR_CLK_OFF		BIT(31)
#define CBCR_CLK_ENABLE		BIT(0)

/* Display clock controller 0 */
#define DISPCC_BASE		0x0af00000
#define DISPCC_MDSS_MDP_CBCR	(DISPCC_BASE + 0x800c)
#define DISPCC_MDSS_BYTE0_CBCR	(DISPCC_BASE + 0x8034)
#define DISPCC_MDSS_ESC0_CBCR	(DISPCC_BASE + 0x8044)
#define DISPCC_MDSS_AHB_CBCR	(DISPCC_BASE + 0x8084)
#define DISPCC_MDSS_CORE_GDSCR	(DISPCC_BASE + 0x9000)
#define DISPCC_MDSS_CORE_CFG	(DISPCC_MDSS_CORE_GDSCR + 0x4)

#define GDSC_PWR_ON		BIT(31)
#define GDSC_RETAIN_FF		BIT(11)
#define GDSC_HW_CONTROL		BIT(1)
#define GDSC_SW_COLLAPSE	BIT(0)
#define GDSC_CFG_PWR_UP_DONE	BIT(16)

/* Display subsystem */
#define MDSS_HW_VERSION		0x0ae00000
#define DPU_HW_VERSION		0x0ae01000
#define DSI0_HW_VERSION		0x0ae94000

/* RPMh, apps RSC DRV2 */
#define RSC_DRV_BASE		0x18220000
#define RSC_DRV_ID		2
#define RSC_TCS_OFFSET		0xd00
#define RSC_NUM_ACTIVE_TCS	2
#define RSC_PRNT_CHLD_CONFIG	0x0c
#define RSC_IRQ_CLEAR		0x08

#define CMD_MSGID_LEN		8
#define CMD_MSGID_RESP_REQ	BIT(8)
#define CMD_MSGID_WRITE		BIT(16)
#define CMD_STATUS_ISSUED	BIT(8)
#define CMD_STATUS_COMPL	BIT(16)
#define TCS_AMC_MODE_ENABLE	BIT(16)
#define TCS_AMC_MODE_TRIGGER	BIT(24)

/* Command DB */
#define CMD_DB_BASE		0x90860000
#define CMD_DB_MAX_SLV		8

#define RPMH_LEVEL_SVS_L1	192

/* Bus Clock Manager vote */
#define BCM_CMD(x, y)		(BIT(30) | BIT(29) | ((x) & 0x3fff) << 14 | \
				 ((y) & 0x3fff))
#define BCM_VOTE_KBPS		1000000	/* ~1 GB/s, enough for 1080p60 */

/* i2c12 = QUP1 SE4 on gpio45/46 */
#define QUP1_WRAPPER_BASE	0x00ac0000
#define I2C12_BASE		0x00a90000
#define TLMM_BASE		0x0f100000
#define TLMM_GPIO_CFG(n)	(TLMM_BASE + (n) * 0x1000)
#define TLMM_FUNC_SHIFT		2
#define TLMM_PULL_UP		3
#define TLMM_OE			BIT(9)
#define TLMM_IN			BIT(0)
#define I2C12_FUNC		1
#define I2C12_SDA		45
#define I2C12_SCL		46
#define BB_DELAY_US		5

#define SE_I2C_TX_TRANS_LEN	0x26c
#define SE_I2C_RX_TRANS_LEN	0x270
#define SE_I2C_SCL_COUNTERS	0x278
#define I2C_WRITE		0x1
#define I2C_READ		0x2
#define I2C_STOP_STRETCH	BIT(2)
#define I2C_SLV_ADDR_SHFT	9
#define I2C_TIMEOUT_MS		100

#define ADV7535_ADDR		0x3d

/* Apps SMMU */
#define SMMU_BASE		0x15000000
#define SMMU_SCR0		0x000
#define SMMU_IDR0		0x020
#define SMMU_IDR1		0x024
#define SMMU_SMR(n)		(0x800 + (n) * 4)
#define SMMU_S2CR(n)		(0xc00 + (n) * 4)
#define SMMU_CBAR(n)		((n) * 4)		/* in GR1 */
#define SMMU_CBA2R(n)		(0x800 + (n) * 4)	/* in GR1 */
#define SMMU_CB_SCTLR		0x00
#define SMMU_CB_TTBR0		0x20
#define SMMU_CB_TTBR1		0x28
#define SMMU_CB_TCR		0x30
#define SMMU_CB_MAIR0		0x38
#define SMMU_CB_MAIR1		0x3c
#define SMMU_SCTLR_CFCFG	BIT(7)
#define SMMU_SCTLR_CFIE		BIT(6)
#define SMMU_SCTLR_CFRE		BIT(5)
#define SMMU_CBAR_TYPE_S2_TRANS	0
#define SMMU_CBAR_TYPE_S1_BYPASS 1	/* S1 translate, S2 bypass */
#define SMMU_VMID_UNUSED	0xff
#define SMMU_SMR_VALID		BIT(31)
#define MDSS_SID		0x1000
#define MDSS_SID_MASK		0x402

/* Context bank used for the MDSS SID, set by 'smmu map' */
static ulong smmu_cb_base;

/*
 * Stream mapping created by do_smmu_map(), with the values it replaced, so
 * that mdss_display_stop() can remove it again
 */
static struct {
	int smr;
	u32 s2cr, sctlr;
} smmu_saved = { .smr = -1 };

/* The mdssprobe command prints every step; the video driver stays quiet */
static bool mdss_verbose;

#define mdss_log(fmt, ...) do {				\
	if (mdss_verbose)					\
		printf(fmt, ##__VA_ARGS__);			\
	else							\
		log_debug(fmt, ##__VA_ARGS__);			\
} while (0)

static u32 rd(ulong addr)
{
	return readl((void __iomem *)addr);
}

static void wr(ulong addr, u32 val)
{
	writel(val, (void __iomem *)addr);
}

static void show(const char *name, ulong addr)
{
	mdss_log("  %-22s @%08lx ... ", name, addr);
	mdss_log("%08x\n", rd(addr));
}

/* Wait for (reg & mask) == val, returning the final register value */
static int poll(ulong addr, u32 mask, u32 val, uint us, u32 *lastp)
{
	u32 reg;

	do {
		reg = rd(addr);
		if ((reg & mask) == val)
			break;
		udelay(1);
	} while (us--);
	if (lastp)
		*lastp = reg;

	return (reg & mask) == val ? 0 : -ETIMEDOUT;
}

static int branch_enable(const char *name, ulong cbcr)
{
	u32 val;
	int ret;

	mdss_log("  enable %-15s @%08lx ... ", name, cbcr);
	wr(cbcr, rd(cbcr) | CBCR_CLK_ENABLE);
	ret = poll(cbcr, CBCR_CLK_OFF, 0, 1000, &val);
	mdss_log("%08x %s\n", val, ret ? "STILL OFF" : "on");

	return ret;
}

/* Command DB lookup */

struct cmd_db_entry {
	u8 id[8];
	__le32 priority[2];
	__le32 addr;
	__le16 len;
	__le16 offset;
};

struct cmd_db_rsc {
	__le16 slv_id;
	__le16 header_offset;
	__le16 data_offset;
	__le16 cnt;
	__le16 version;
	__le16 reserved[3];
};

struct cmd_db_header {
	__le32 version;
	u8 magic[4];
	struct cmd_db_rsc rsc[CMD_DB_MAX_SLV];
	__le32 checksum;
	__le32 reserved;
	u8 data[];
};

static const u8 cmd_db_magic[] = { 0xdb, 0x30, 0x03, 0x0c };

static int cmd_db_find(const char *name, u32 *addrp, const void **auxp,
		       uint *lenp)
{
	struct cmd_db_header *hdr = (void *)CMD_DB_BASE;
	char query[8] __nonstring;
	int i, j;

	if (memcmp(hdr->magic, cmd_db_magic, sizeof(cmd_db_magic)))
		return -EINVAL;

	strncpy(query, name, sizeof(query));
	for (i = 0; i < CMD_DB_MAX_SLV; i++) {
		struct cmd_db_rsc *rsc = &hdr->rsc[i];
		struct cmd_db_entry *ent;

		if (!rsc->slv_id)
			break;
		ent = (void *)(hdr->data + le16_to_cpu(rsc->header_offset));
		for (j = 0; j < le16_to_cpu(rsc->cnt); j++, ent++) {
			if (strncmp((char *)ent->id, query, sizeof(query)))
				continue;
			*addrp = le32_to_cpu(ent->addr);
			if (auxp)
				*auxp = hdr->data + le16_to_cpu(rsc->data_offset) +
					le16_to_cpu(ent->offset);
			if (lenp)
				*lenp = le16_to_cpu(ent->len);
			return 0;
		}
	}

	return -ENOENT;
}

/* Minimal RPMh RSC access, using an idle active-only TCS of DRV2 */

struct rsc_regs {
	uint tcs_size;
	uint cmd_size;
	uint wait_for_cmpl;
	uint control;
	uint status;
	uint cmd_enable;
	uint cmd_msgid;
	uint cmd_addr;
	uint cmd_data;
	uint cmd_status;
	uint cmd_resp;
};

static const struct rsc_regs rsc_v2 = {
	672, 20, 0x10, 0x14, 0x18, 0x1c, 0x30, 0x34, 0x38, 0x3c, 0x40
};

static const struct rsc_regs rsc_v3 = {
	672, 24, 0x20, 0x24, 0x28, 0x2c, 0x34, 0x38, 0x3c, 0x40, 0x44
};

static const struct rsc_regs *rsc;

static ulong tcs_reg(int tcs, uint reg)
{
	return RSC_DRV_BASE + RSC_TCS_OFFSET + rsc->tcs_size * tcs + reg;
}

static ulong tcs_cmd(int tcs, uint reg, int cmd)
{
	return tcs_reg(tcs, reg) + rsc->cmd_size * cmd;
}

static void rsc_init(bool verbose)
{
	u32 id = rd(RSC_DRV_BASE);
	u32 cfg = rd(RSC_DRV_BASE + RSC_PRNT_CHLD_CONFIG);
	uint major = (id >> 16) & 0xff, minor = (id >> 8) & 0xff;
	int i;

	rsc = major >= 3 ? &rsc_v3 : &rsc_v2;
	if (!verbose)
		return;

	mdss_log("  RSC DRV%d v%u.%u, %u TCSs, %u cmds/TCS\n", RSC_DRV_ID,
	       major, minor, (cfg >> (6 * RSC_DRV_ID)) & 0x3f,
	       (cfg >> 27) & 0x1f);
	for (i = 0; i < RSC_NUM_ACTIVE_TCS; i++)
		mdss_log("  active TCS%d: status %x control %08x cmd_enable %x\n",
		       i, rd(tcs_reg(i, rsc->status)),
		       rd(tcs_reg(i, rsc->control)),
		       rd(tcs_reg(i, rsc->cmd_enable)));
}

static void tcs_ctrl_sync(int tcs, u32 val)
{
	wr(tcs_reg(tcs, rsc->control), val);
	poll(tcs_reg(tcs, rsc->control), ~0, val, 1000, NULL);
}

static int rsc_xfer(u32 addr, u32 data, bool is_read, u32 *respp)
{
	u32 msgid = CMD_MSGID_LEN | CMD_MSGID_RESP_REQ;
	u32 status;
	int tcs, ret;

	for (tcs = 0; tcs < RSC_NUM_ACTIVE_TCS; tcs++)
		if (rd(tcs_reg(tcs, rsc->status)))
			break;
	if (tcs == RSC_NUM_ACTIVE_TCS) {
		mdss_log("  no idle active TCS\n");
		return -EBUSY;
	}

	if (!is_read)
		msgid |= CMD_MSGID_WRITE;
	wr(tcs_cmd(tcs, rsc->cmd_msgid, 0), msgid);
	wr(tcs_cmd(tcs, rsc->cmd_addr, 0), addr);
	if (!is_read)
		wr(tcs_cmd(tcs, rsc->cmd_data, 0), data);
	wr(tcs_reg(tcs, rsc->cmd_enable), BIT(0));
	wr(tcs_reg(tcs, rsc->wait_for_cmpl), BIT(0));

	/* Clear any previous trigger, then enable AMC mode and trigger */
	tcs_ctrl_sync(tcs, rd(tcs_reg(tcs, rsc->control)) &
		      ~TCS_AMC_MODE_TRIGGER);
	tcs_ctrl_sync(tcs, 0);
	tcs_ctrl_sync(tcs, TCS_AMC_MODE_ENABLE);
	wr(tcs_reg(tcs, rsc->control),
	   TCS_AMC_MODE_ENABLE | TCS_AMC_MODE_TRIGGER);

	ret = poll(tcs_cmd(tcs, rsc->cmd_status, 0), CMD_STATUS_COMPL,
		   CMD_STATUS_COMPL, 100000, &status);
	if (respp)
		*respp = rd(tcs_cmd(tcs, rsc->cmd_resp, 0));

	/* Release the TCS */
	tcs_ctrl_sync(tcs, TCS_AMC_MODE_ENABLE);
	tcs_ctrl_sync(tcs, 0);
	wr(tcs_reg(tcs, rsc->cmd_enable), 0);
	wr(RSC_DRV_BASE + RSC_TCS_OFFSET + RSC_IRQ_CLEAR, BIT(tcs));

	mdss_log("  TCS%d %s %05x data %x: cmd_status %08x%s\n", tcs,
	       is_read ? "read " : "write", addr, data, status,
	       ret ? " TIMEOUT" : "");

	return ret;
}

/* mdssprobe info */

static const char *const cmd_db_names[] __maybe_unused = {
	"mmcx.lvl", "cx.lvl", "mx.lvl", "mxc.lvl", "ldoa5", "ldoa4",
	"MM0", "MM1", "SH0", "MC0", "ACV",
};

static __maybe_unused int do_info(void)
{
	uint i;

	mdss_log("GCC:\n");
	show("DISP_AHB_CBCR", GCC_DISP_AHB_CBCR);
	show("DISP_XO_CBCR", GCC_DISP_XO_CBCR);
	show("DISP_HF_AXI_CBCR", GCC_DISP_HF_AXI_CBCR);
	show("APCS_VOTE", GCC_APCS_VOTE);
	show("QUP1_CORE_CBCR", GCC_QUP1_CORE_CBCR);
	show("QUP1_CORE2X_CBCR", GCC_QUP1_CORE2X_CBCR);
	show("QUP1_S4_CBCR", GCC_QUP1_S4_CBCR);
	show("QUP1_S4_CMD_RCGR", GCC_QUP1_S4_CMD_RCGR);
	show("QUP1_S4_CFG_RCGR", GCC_QUP1_S4_CMD_RCGR + 4);

	mdss_log("TLMM:\n");
	show("gpio45 cfg", TLMM_GPIO_CFG(45));
	show("gpio46 cfg", TLMM_GPIO_CFG(46));
	show("gpio93 cfg", TLMM_GPIO_CFG(93));
	show("gpio93 in/out", TLMM_GPIO_CFG(93) + 4);

	mdss_log("RPMh:\n");
	rsc_init(true);

	mdss_log("Command DB @%08x:\n", CMD_DB_BASE);
	for (i = 0; i < ARRAY_SIZE(cmd_db_names); i++) {
		u32 addr;

		if (cmd_db_find(cmd_db_names[i], &addr, NULL, NULL))
			mdss_log("  %-10s not found\n", cmd_db_names[i]);
		else
			mdss_log("  %-10s %05x\n", cmd_db_names[i], addr);
	}

	return 0;
}

/* mdssprobe power */

static int mmcx_vote(void)
{
	const __le16 *levels;
	uint len, i, hlvl = 0;
	u32 addr, resp;
	int ret;

	ret = cmd_db_find("mmcx.lvl", &addr, (const void **)&levels, &len);
	if (ret) {
		mdss_log("  mmcx.lvl not in cmd-db (%d)\n", ret);
		return ret;
	}
	mdss_log("  mmcx.lvl @%05x, levels:", addr);
	for (i = 0; i < len / 2; i++) {
		uint vlvl = le16_to_cpu(levels[i]);

		if (i && !vlvl)
			break;
		mdss_log(" %u", vlvl);
		if (!hlvl && vlvl >= RPMH_LEVEL_SVS_L1)
			hlvl = i;
	}
	mdss_log("\n");
	if (!hlvl) {
		mdss_log("  no SVS_L1 level found\n");
		return -EINVAL;
	}

	rsc_xfer(addr, 0, true, &resp);
	mdss_log("  current DRV%d MMCX vote: hlvl %u\n", RSC_DRV_ID, resp);

	mdss_log("  voting MMCX hlvl %u (vlvl %u)\n", hlvl,
	       le16_to_cpu(levels[hlvl]));

	return rsc_xfer(addr, hlvl, false, NULL);
}

static int gdsc_enable(void)
{
	u32 val;
	int ret;

	mdss_log("  enable MDSS_CORE_GDSC ... ");
	val = rd(DISPCC_MDSS_CORE_GDSCR);
	val &= ~(GDSC_SW_COLLAPSE | GDSC_HW_CONTROL);
	val |= GDSC_RETAIN_FF;
	wr(DISPCC_MDSS_CORE_GDSCR, val);
	ret = poll(DISPCC_MDSS_CORE_CFG, GDSC_CFG_PWR_UP_DONE,
		   GDSC_CFG_PWR_UP_DONE, 1000, NULL);
	mdss_log("gdscr %08x cfg %08x %s\n", rd(DISPCC_MDSS_CORE_GDSCR),
	       rd(DISPCC_MDSS_CORE_CFG), ret ? "TIMEOUT" : "on");

	return ret;
}

static int do_power(bool with_dsi)
{
	int ret;

	mdss_log("Step 1: RPMh MMCX vote\n");
	rsc_init(true);
	ret = mmcx_vote();
	if (ret) {
		mdss_log("MMCX vote failed, stopping before touching dispcc\n");
		return ret;
	}

	mdss_log("Step 2: GCC display clocks\n");
	branch_enable("DISP_AHB", GCC_DISP_AHB_CBCR);
	branch_enable("DISP_XO", GCC_DISP_XO_CBCR);
	branch_enable("DISP_HF_AXI", GCC_DISP_HF_AXI_CBCR);

	mdss_log("Step 3: dispcc state (hangs here = MMCX/AHB still off)\n");
	show("MDSS_CORE_GDSCR", DISPCC_MDSS_CORE_GDSCR);
	show("MDSS_CORE_CFG_GDSCR", DISPCC_MDSS_CORE_CFG);
	show("MDSS_AHB_CBCR", DISPCC_MDSS_AHB_CBCR);
	show("MDSS_MDP_CBCR", DISPCC_MDSS_MDP_CBCR);
	show("MDSS_BYTE0_CBCR", DISPCC_MDSS_BYTE0_CBCR);
	show("MDSS_ESC0_CBCR", DISPCC_MDSS_ESC0_CBCR);

	mdss_log("Step 4: MDSS power and clocks\n");
	ret = gdsc_enable();
	if (ret)
		return ret;
	if (branch_enable("MDSS_AHB", DISPCC_MDSS_AHB_CBCR) ||
	    branch_enable("MDSS_MDP", DISPCC_MDSS_MDP_CBCR))
		return -EIO;

	mdss_log("Step 5: hardware versions\n");
	show("MDSS_HW_VERSION", MDSS_HW_VERSION);
	show("DPU_HW_VERSION", DPU_HW_VERSION);
	if (with_dsi)
		show("DSI0_HW_VERSION", DSI0_HW_VERSION);
	else
		mdss_log("  skipping DSI0 (use 'mdssprobe power dsi')\n");

	mdss_log("done\n");

	return 0;
}

/* mdssprobe bw */

struct bcm_db {
	__le32 unit;
	__le16 width;
	u8 vcd;
	u8 reserved;
};

static int bcm_vote(const char *name)
{
	const struct bcm_db *db;
	u32 addr, resp, vote;
	uint len, unit;
	int ret;

	ret = cmd_db_find(name, &addr, (const void **)&db, &len);
	if (ret || len < sizeof(*db)) {
		mdss_log("  %s not in cmd-db (%d)\n", name, ret);
		return -ENOENT;
	}
	unit = le32_to_cpu(db->unit);
	mdss_log("  %s @%05x unit %u width %u vcd %u\n", name, addr, unit,
	       le16_to_cpu(db->width), db->vcd);
	if (!unit)
		return -EINVAL;

	rsc_xfer(addr, 0, true, &resp);
	mdss_log("  current DRV%d %s vote: x %u y %u (%08x)\n", RSC_DRV_ID, name,
	       (resp >> 14) & 0x3fff, resp & 0x3fff, resp);

	vote = min_t(u64, (u64)BCM_VOTE_KBPS * 1000 / unit, 0x3fff);
	if (!vote)
		vote = 1;
	mdss_log("  voting %s x=y=%u\n", name, vote);

	return rsc_xfer(addr, BCM_CMD(vote, vote), false, NULL);
}

static int do_bw(void)
{
	u32 val;
	int ret;

	rsc_init(true);
	mdss_log("Step 1: BCM votes\n");
	ret = bcm_vote("MM0");
	if (!ret)
		ret = bcm_vote("MM1");
	if (ret)
		return ret;

	mdss_log("Step 2: DISP_HF_AXI clock\n");
	wr(GCC_DISP_HF_AXI_CBCR, rd(GCC_DISP_HF_AXI_CBCR) | CBCR_CLK_ENABLE);
	ret = poll(GCC_DISP_HF_AXI_CBCR, CBCR_CLK_OFF, 0, 1000, &val);
	mdss_log("  DISP_HF_AXI_CBCR %08x %s\n", val, ret ? "STILL OFF" : "on");

	return ret;
}

/* Bit-banged I2C on gpio45 (SDA) / gpio46 (SCL), open drain via OE */

static void bb_line(uint gpio, bool high)
{
	/* Low: drive 0. High: release and let the pull-up raise the line */
	wr(TLMM_GPIO_CFG(gpio), TLMM_PULL_UP | (high ? 0 : TLMM_OE));
	udelay(BB_DELAY_US);
}

static bool bb_get(uint gpio)
{
	return rd(TLMM_GPIO_CFG(gpio) + 4) & TLMM_IN;
}

static void bb_scl_high(void)
{
	int timeout = 1000;

	bb_line(I2C12_SCL, true);
	/* Allow clock stretching */
	while (!bb_get(I2C12_SCL) && timeout--)
		udelay(1);
}

static void bb_init(void)
{
	int i;

	/* GPIO function, output value 0, lines released */
	wr(TLMM_GPIO_CFG(I2C12_SDA) + 4, 0);
	wr(TLMM_GPIO_CFG(I2C12_SCL) + 4, 0);
	bb_line(I2C12_SDA, true);
	bb_line(I2C12_SCL, true);

	/*
	 * Bus recovery: the devices on i2c12 are not reset with the SoC, so
	 * a reboot in the middle of a transfer can leave one holding SDA low.
	 * Clock it out of the transfer, then send a STOP.
	 */
	for (i = 0; i < 9 && !bb_get(I2C12_SDA); i++) {
		bb_line(I2C12_SCL, false);
		bb_scl_high();
	}
	bb_line(I2C12_SCL, false);
	bb_line(I2C12_SDA, false);
	bb_scl_high();
	bb_line(I2C12_SDA, true);
}

static void bb_start(void)
{
	bb_line(I2C12_SDA, true);
	bb_scl_high();
	bb_line(I2C12_SDA, false);
	bb_line(I2C12_SCL, false);
}

static void bb_stop(void)
{
	bb_line(I2C12_SDA, false);
	bb_scl_high();
	bb_line(I2C12_SDA, true);
}

/* Returns true if the byte was acked */
static bool bb_write(u8 byte)
{
	bool ack;
	int i;

	for (i = 7; i >= 0; i--) {
		bb_line(I2C12_SDA, byte & BIT(i));
		bb_scl_high();
		bb_line(I2C12_SCL, false);
	}
	bb_line(I2C12_SDA, true);
	bb_scl_high();
	ack = !bb_get(I2C12_SDA);
	bb_line(I2C12_SCL, false);

	return ack;
}

static u8 bb_read(bool ack)
{
	u8 byte = 0;
	int i;

	bb_line(I2C12_SDA, true);
	for (i = 7; i >= 0; i--) {
		bb_scl_high();
		if (bb_get(I2C12_SDA))
			byte |= BIT(i);
		bb_line(I2C12_SCL, false);
	}
	bb_line(I2C12_SDA, !ack);
	bb_scl_high();
	bb_line(I2C12_SCL, false);
	bb_line(I2C12_SDA, true);

	return byte;
}

static int bb_read_reg(u8 addr, u8 reg, u8 *valp)
{
	int ret = -EREMOTEIO;

	bb_start();
	if (!bb_write(addr << 1) || !bb_write(reg))
		goto out;
	bb_start();
	if (!bb_write(addr << 1 | 1))
		goto out;
	*valp = bb_read(false);
	ret = 0;
out:
	bb_stop();

	return ret;
}

static int bb_write_reg(u8 addr, u8 reg, u8 val)
{
	int ret = -EREMOTEIO;

	bb_start();
	if (bb_write(addr << 1) && bb_write(reg) && bb_write(val))
		ret = 0;
	bb_stop();

	return ret;
}

static __maybe_unused int do_i2c_bb(bool scan)
{
	static const u8 regs[] = { 0x00, 0xf5, 0xf6 };
	uint i;

	mdss_log("Bit-banged i2c on gpio%d (SDA) / gpio%d (SCL)\n", I2C12_SDA,
	       I2C12_SCL);
	bb_init();
	mdss_log("  idle: SDA %d SCL %d (both should be 1)\n",
	       bb_get(I2C12_SDA), bb_get(I2C12_SCL));
	if (!bb_get(I2C12_SDA) || !bb_get(I2C12_SCL)) {
		mdss_log("bus stuck low\n");
		return -EIO;
	}

	if (scan) {
		mdss_log("  devices:");
		for (i = 0x08; i < 0x78; i++) {
			bool ack;

			bb_start();
			ack = bb_write(i << 1);
			bb_stop();
			if (ack)
				mdss_log(" %02x", i);
		}
		mdss_log("\n");
		return 0;
	}

	mdss_log("ADV7535 @%02x (expect 00=14, f5=75, f6=33)\n", ADV7535_ADDR);
	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		u8 val = 0;

		if (bb_read_reg(ADV7535_ADDR, regs[i], &val))
			mdss_log("  reg %02x: no ack\n", regs[i]);
		else
			mdss_log("  reg %02x: %02x\n", regs[i], val);
	}

	return 0;
}

/* mdssprobe i2c geni */

static __maybe_unused void geni_config_packing(ulong base)
{
	u32 cfg[4] = {0};
	int i, idx = 7;

	/* 8 bits per word, 4 words per FIFO entry, MSB first */
	for (i = 0; i < 4; i++) {
		cfg[i] = idx << 5 | 1 << 4 | 7 << 1;
		idx = (i + 1) * 8 + 7;
	}
	cfg[3] |= BIT(0);
	wr(base + SE_GENI_TX_PACKING_CFG0, cfg[0] | cfg[1] << 10);
	wr(base + SE_GENI_TX_PACKING_CFG1, cfg[2] | cfg[3] << 10);
	wr(base + SE_GENI_RX_PACKING_CFG0, cfg[0] | cfg[1] << 10);
	wr(base + SE_GENI_RX_PACKING_CFG1, cfg[2] | cfg[3] << 10);
}

static __maybe_unused int geni_i2c_init(ulong base, uint *tx_depthp)
{
	u32 ver, val, depth_mask;
	uint tx_depth;

	ver = rd(QUP1_WRAPPER_BASE + QUP_HW_VER_REG);
	if ((GENI_SE_VERSION_MAJOR(ver) == 3 &&
	     GENI_SE_VERSION_MINOR(ver) >= 10) ||
	    GENI_SE_VERSION_MAJOR(ver) > 3)
		depth_mask = TX_FIFO_DEPTH_MSK_256_BYTES;
	else
		depth_mask = TX_FIFO_DEPTH_MSK;
	tx_depth = (rd(base + SE_HW_PARAM_0) & depth_mask) >>
		TX_FIFO_DEPTH_SHFT;
	mdss_log("  QUP version %08x, TX FIFO depth %u\n", ver, tx_depth);
	if (!tx_depth)
		return -EINVAL;

	wr(base + SE_GSI_EVENT_EN, 0);
	wr(base + SE_GENI_M_IRQ_CLEAR, 0xffffffff);
	wr(base + SE_GENI_S_IRQ_CLEAR, 0xffffffff);
	wr(base + SE_IRQ_EN, 0xffffffff);
	wr(base + GENI_CGC_CTRL, rd(base + GENI_CGC_CTRL) | DEFAULT_CGC_EN);
	wr(base + GENI_OUTPUT_CTRL, DEFAULT_IO_OUTPUT_CTRL_MSK);
	wr(base + GENI_FORCE_DEFAULT_REG, FORCE_DEFAULT);
	wr(base + SE_GENI_DMA_MODE_EN,
	   rd(base + SE_GENI_DMA_MODE_EN) & ~GENI_DMA_MODE_EN);
	wr(base + SE_GENI_RX_WATERMARK_REG, tx_depth - 1);
	wr(base + SE_GENI_RX_RFR_WATERMARK_REG, tx_depth);
	val = rd(base + SE_GENI_M_IRQ_EN) | M_COMMON_GENI_M_IRQ_EN |
		M_CMD_DONE_EN | M_TX_FIFO_WATERMARK_EN |
		M_RX_FIFO_WATERMARK_EN | M_RX_FIFO_LAST_EN;
	wr(base + SE_GENI_M_IRQ_EN, val);
	wr(base + SE_GENI_S_IRQ_EN,
	   rd(base + SE_GENI_S_IRQ_EN) | S_COMMON_GENI_S_IRQ_EN);
	geni_config_packing(base);
	wr(base + SE_GENI_BYTE_GRAN, 0);

	/* 100 kHz from the 19.2 MHz XO: div 7, high 10, low 11, cycle 26 */
	wr(base + SE_GENI_CLK_SEL, 0);
	wr(base + GENI_SER_M_CLK_CFG, 7 << CLK_DIV_SHFT | SER_CLK_EN);
	wr(base + SE_I2C_SCL_COUNTERS, 10 << 20 | 11 << 10 | 26);
	*tx_depthp = tx_depth;

	return 0;
}

#define M_ERR_MASK	(M_CMD_ABORT_EN | M_CMD_OVERRUN_EN | \
			 M_ILLEGAL_CMD_EN | M_CMD_FAILURE_EN | \
			 M_GP_IRQ_1_EN | M_GP_IRQ_3_EN | M_GP_IRQ_4_EN)

static __maybe_unused int geni_wait(ulong base, u8 *rxp)
{
	ulong start = get_timer(0);
	bool got = false;
	u32 status;

	while (get_timer(start) < I2C_TIMEOUT_MS) {
		status = rd(base + SE_GENI_M_IRQ_STATUS);
		if (status & M_ERR_MASK) {
			wr(base + SE_GENI_M_IRQ_CLEAR, status);
			wr(base + SE_GENI_TX_WATERMARK_REG, 0);
			return -EREMOTEIO;
		}
		if (rxp && !got &&
		    (rd(base + SE_GENI_RX_FIFO_STATUS) & RX_FIFO_WC_MSK)) {
			*rxp = rd(base + SE_GENI_RX_FIFOn) & 0xff;
			got = true;
		}
		if (status & M_TX_FIFO_WATERMARK_EN)
			wr(base + SE_GENI_TX_WATERMARK_REG, 0);
		wr(base + SE_GENI_M_IRQ_CLEAR, status);
		if (status & M_CMD_DONE_EN)
			return rxp && !got ? -ENODATA : 0;
	}

	/* Abort the stuck command */
	wr(base + SE_GENI_M_CMD_CTRL_REG, M_GENI_CMD_ABORT);
	poll(base + SE_GENI_M_IRQ_STATUS, M_CMD_ABORT_EN, M_CMD_ABORT_EN,
	     10000, &status);
	wr(base + SE_GENI_M_IRQ_CLEAR, status);

	return -ETIMEDOUT;
}

static __maybe_unused int geni_i2c_read_reg(ulong base, u8 addr, u8 reg, u8 *valp)
{
	u32 param = addr << I2C_SLV_ADDR_SHFT;
	int ret;

	wr(base + SE_GENI_M_IRQ_CLEAR, 0xffffffff);

	/* Write the register offset, keeping the bus for a repeated start */
	wr(base + SE_I2C_TX_TRANS_LEN, 1);
	wr(base + SE_GENI_M_CMD0, I2C_WRITE << M_OPCODE_SHFT |
	   ((param | I2C_STOP_STRETCH) & M_PARAMS_MSK));
	wr(base + SE_GENI_TX_FIFOn, reg);
	ret = geni_wait(base, NULL);
	if (ret)
		return ret;

	wr(base + SE_I2C_RX_TRANS_LEN, 1);
	wr(base + SE_GENI_M_CMD0, I2C_READ << M_OPCODE_SHFT |
	   (param & M_PARAMS_MSK));

	return geni_wait(base, valp);
}

static __maybe_unused int do_i2c(void)
{
	static const u8 regs[] = { 0x00, 0xf5, 0xf6 };
	u32 vote, proto;
	uint tx_depth, i;
	int ret;

	mdss_log("Step 1: QUP1 clocks\n");
	vote = GCC_VOTE_QUP1_CORE2X | GCC_VOTE_QUP1_CORE |
		GCC_VOTE_QUP1_M_AHB | GCC_VOTE_QUP1_S_AHB | GCC_VOTE_QUP1_S4;
	wr(GCC_APCS_VOTE, rd(GCC_APCS_VOTE) | vote);
	show("APCS_VOTE", GCC_APCS_VOTE);
	udelay(100);
	show("QUP1_M_AHB_CBCR", GCC_QUP1_M_AHB_CBCR);
	show("QUP1_S_AHB_CBCR", GCC_QUP1_S_AHB_CBCR);
	show("QUP1_CORE_CBCR", GCC_QUP1_CORE_CBCR);
	show("QUP1_CORE2X_CBCR", GCC_QUP1_CORE2X_CBCR);
	show("QUP1_S4_CBCR", GCC_QUP1_S4_CBCR);
	show("QUP1_S4_CFG_RCGR", GCC_QUP1_S4_CMD_RCGR + 4);
	if (rd(GCC_QUP1_S4_CBCR) & CBCR_CLK_OFF) {
		mdss_log("i2c12 clock did not turn on, stopping\n");
		return -EIO;
	}

	mdss_log("Step 2: pinmux gpio45/46 to qup1_se4\n");
	for (i = 45; i <= 46; i++) {
		wr(TLMM_GPIO_CFG(i),
		   I2C12_FUNC << TLMM_FUNC_SHIFT | TLMM_PULL_UP);
		show(i == 45 ? "gpio45 cfg" : "gpio46 cfg", TLMM_GPIO_CFG(i));
	}

	mdss_log("Step 3: GENI serial engine\n");
	show("GENI_FW_REVISION_RO", I2C12_BASE + GENI_FW_REVISION_RO);
	show("GENI_IF_DISABLE_RO", I2C12_BASE + GENI_IF_DISABLE_RO);
	proto = (rd(I2C12_BASE + GENI_FW_REVISION_RO) & FW_REV_PROTOCOL_MSK) >>
		FW_REV_PROTOCOL_SHFT;
	if (proto != GENI_SE_I2C) {
		mdss_log("SE protocol %u is not I2C (firmware not loaded?)\n",
		       proto);
		return -ENXIO;
	}
	if (rd(I2C12_BASE + GENI_IF_DISABLE_RO) & FIFO_IF_DISABLE) {
		mdss_log("FIFO mode disabled\n");
		return -ENXIO;
	}
	ret = geni_i2c_init(I2C12_BASE, &tx_depth);
	if (ret)
		return ret;

	mdss_log("Step 4: ADV7535 @%02x (expect 00=14, f5=75, f6=33)\n",
	       ADV7535_ADDR);
	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		u8 val = 0;

		ret = geni_i2c_read_reg(I2C12_BASE, ADV7535_ADDR, regs[i],
					&val);
		if (ret)
			mdss_log("  reg %02x: error %d\n", regs[i], ret);
		else
			mdss_log("  reg %02x: %02x\n", regs[i], val);
	}

	return 0;
}

/* mdssprobe smmu */

static __maybe_unused int do_smmu(void)
{
	u32 idr0, smr, s2cr, id, mask;
	uint i, num;

	mdss_log("Apps SMMU (hangs/aborts here = owned by the hypervisor)\n");
	show("sCR0", SMMU_BASE + SMMU_SCR0);
	show("sIDR0", SMMU_BASE + SMMU_IDR0);
	idr0 = rd(SMMU_BASE + SMMU_IDR0);
	num = idr0 & 0xff;
	mdss_log("  %u stream match registers, looking for SID %x\n", num,
	       MDSS_SID);
	for (i = 0; i < num; i++) {
		smr = rd(SMMU_BASE + SMMU_SMR(i));
		id = smr & 0xffff;
		mask = (smr >> 16) & 0x7fff;
		if (!(smr & SMMU_SMR_VALID) ||
		    (id & ~mask) != (MDSS_SID & ~mask))
			continue;
		s2cr = rd(SMMU_BASE + SMMU_S2CR(i));
		mdss_log("  SMR%u %08x S2CR %08x: type %u (0=xlate 1=bypass 2=fault) cb %u\n",
		       i, smr, s2cr, (s2cr >> 16) & 3, s2cr & 0xff);
	}

	return 0;
}

/*
 * Map the MDSS SID to a context bank with translation disabled (S1 bypass,
 * S2 bypass), as drivers/iommu/qcom-hyp-smmu.c does, since bypass S2CRs are
 * not allowed by the hypervisor
 */
static int do_smmu_map(void)
{
	u32 idr1, cbar, type, vmid, val;
	uint num_smr, num_cb, pgshift, cb_pg, i;
	ulong gr1, cb;
	int cbx = -1, smr = -1;

	num_smr = rd(SMMU_BASE + SMMU_IDR0) & 0xff;
	idr1 = rd(SMMU_BASE + SMMU_IDR1);
	num_cb = idr1 & 0xff;
	pgshift = idr1 & BIT(31) ? 16 : 12;
	cb_pg = 1 << (((idr1 >> 28) & 7) + 1);
	gr1 = SMMU_BASE + (1 << pgshift);
	mdss_log("  sIDR1 %08x: %u context banks, page shift %u, CB page %u\n",
	       idr1, num_cb, pgshift, cb_pg);

	for (i = 0; i < num_smr; i++) {
		val = rd(SMMU_BASE + SMMU_SMR(i));
		if ((val & SMMU_SMR_VALID) && (val & 0xffff) == MDSS_SID) {
			cbx = rd(SMMU_BASE + SMMU_S2CR(i)) & 0xff;
			smmu_cb_base = SMMU_BASE +
				((ulong)(cb_pg + cbx) << pgshift);
			mdss_log("  SMR%u already maps SID %x to CB%d\n", i,
			       MDSS_SID, cbx);
			return 0;
		}
	}

	for (i = 0; i < num_cb; i++) {
		cbar = rd(gr1 + SMMU_CBAR(i));
		type = (cbar >> 16) & 3;
		vmid = cbar & 0xff;
		if (type == SMMU_CBAR_TYPE_S2_TRANS ||
		    (type == SMMU_CBAR_TYPE_S1_BYPASS &&
		     vmid == SMMU_VMID_UNUSED)) {
			cbx = i;
			break;
		}
	}
	if (cbx < 0) {
		mdss_log("  no free context bank\n");
		return -ENOSPC;
	}
	mdss_log("  using CB%d (CBAR %08x)\n", cbx, cbar);
	cb = SMMU_BASE + ((ulong)(cb_pg + cbx) << pgshift);
	smmu_saved.sctlr = rd(cb + SMMU_CB_SCTLR);

	cbar &= ~(GENMASK(17, 16) | GENMASK(7, 0));
	cbar |= SMMU_CBAR_TYPE_S1_BYPASS << 16;
	wr(gr1 + SMMU_CBAR(cbx), cbar);
	wr(gr1 + SMMU_CBA2R(cbx), rd(gr1 + SMMU_CBA2R(cbx)) | BIT(0));

	smmu_cb_base = cb;
	wr(cb + SMMU_CB_TTBR0, 0);
	wr(cb + SMMU_CB_TTBR1, 0);
	wr(cb + SMMU_CB_MAIR0, 0);
	wr(cb + SMMU_CB_MAIR1, 0);
	wr(cb + SMMU_CB_SCTLR,
	   SMMU_SCTLR_CFIE | SMMU_SCTLR_CFRE | SMMU_SCTLR_CFCFG);
	wr(cb + SMMU_CB_TCR, 0);

	for (i = 0; i < num_smr; i++) {
		if (!(rd(SMMU_BASE + SMMU_SMR(i)) & SMMU_SMR_VALID)) {
			smr = i;
			break;
		}
	}
	if (smr < 0) {
		mdss_log("  no free stream match register\n");
		return -ENOSPC;
	}
	smmu_saved.smr = smr;
	smmu_saved.s2cr = rd(SMMU_BASE + SMMU_S2CR(smr));
	wr(SMMU_BASE + SMMU_S2CR(smr), cbx);	/* type 0 = translate */
	wr(SMMU_BASE + SMMU_SMR(smr),
	   SMMU_SMR_VALID | MDSS_SID_MASK << 16 | MDSS_SID);
	mb();

	mdss_log("  SMR%d %08x S2CR %08x, CB%d CBAR %08x SCTLR %08x\n", smr,
	       rd(SMMU_BASE + SMMU_SMR(smr)), rd(SMMU_BASE + SMMU_S2CR(smr)),
	       cbx, rd(gr1 + SMMU_CBAR(cbx)), rd(cb + SMMU_CB_SCTLR));
	mdss_log("  (values reading back as written = the writes took effect)\n");

	return 0;
}

/*
 * Display pipeline: DSI0 + ADV7535
 *
 * Without a framebuffer the DPU INTF1 timing engine runs with no pixel
 * source (the screen stays black), which checks the PLLs, DSI PHY and
 * controller, INTF timing and the bridge without SSPP fetch or the SMMU.
 */

/* CEA-861 1280x720@60, positive syncs */
const struct mdss_mode mdss_mode_720p = {
	74250, 1280, 110, 40, 220, 720, 5, 5, 20
};

#define DSI_LANES		4
#define DSI_BPP			24
#define VCO_REF_HZ		19200000ULL
#define VCO_MIN_HZ		600000000ULL

/* RPMh resources */
#define VRM_VOLTAGE		0x0
#define VRM_ENABLE		0x4
#define VRM_MODE		0x8
#define LDO_MODE_HPM		7
#define L5A_MV			1200

#define REFGEN_PWRDWN_CTRL5	0x0891c080

/* dispcc clock sources (CMD_RCGR; CFG_RCGR is +4) and extra branches */
#define DISPCC_PLL0		(DISPCC_BASE + 0x0)
#define DISPCC_MDP_RCGR		(DISPCC_BASE + 0x80bc)
#define DISPCC_PCLK0_RCGR	(DISPCC_BASE + 0x808c)
#define DISPCC_BYTE0_RCGR	(DISPCC_BASE + 0x80ec)
#define DISPCC_ESC0_RCGR	(DISPCC_BASE + 0x8124)
#define DISPCC_BYTE0_DIV	(DISPCC_BASE + 0x8104)
#define DISPCC_PCLK0_CBCR	(DISPCC_BASE + 0x8004)
#define DISPCC_BYTE0_INTF_CBCR	(DISPCC_BASE + 0x8038)
#define DISPCC_XO_CBCR		(DISPCC_BASE + 0xc054)
#define DISPCC_SLEEP_CBCR	(DISPCC_BASE + 0xc070)
#define RCG_UPDATE		BIT(0)
#define RCG_SRC_SHIFT		8
#define SRC_XO			0
#define SRC_DSI0_PIXEL		1
#define SRC_DSI0_BYTE		2
#define SRC_DISP_PLL0		1	/* MDP RCG parent map */

/* Lucid EVO PLL */
#define PLL_MODE		0x00
#define PLL_OPMODE		0x04
#define PLL_L_VAL		0x10
#define PLL_ALPHA_VAL		0x14
#define PLL_USER_CTL		0x18
#define PLL_USER_CTL_U		0x1c
#define PLL_CONFIG_CTL		0x20
#define PLL_CONFIG_CTL_U	0x24
#define PLL_CONFIG_CTL_U1	0x28
#define PLL_TEST_CTL		0x2c
#define PLL_TEST_CTL_U		0x30
#define PLL_TEST_CTL_U1		0x34
#define PLL_OUTCTRL		BIT(0)
#define PLL_RESET_N		BIT(2)
#define PLL_LOCK_DET		BIT(31)
#define PLL_RUN			1
#define PLL_OUT_MASK		7
#define PLL_CAL_L_VAL		0x44

/* DSI0 PHY (5nm, v4.2) */
#define PHY_CMN			0x0ae94400
#define PHY_LN			0x0ae94600
#define PHY_PLL			0x0ae94900

#define CMN_REVISION_ID0	0x000
#define CMN_CLK_CFG0		0x010
#define CMN_CLK_CFG1		0x014
#define CMN_RBUF_CTRL		0x01c
#define CMN_VREG_CTRL_0		0x020
#define CMN_CTRL_0		0x024
#define CMN_CTRL_2		0x02c
#define CMN_CTRL_3		0x030
#define CMN_LANE_CFG0		0x034
#define CMN_LANE_CFG1		0x038
#define CMN_PLL_CNTRL		0x03c
#define CMN_LANE_CTRL0		0x0a0
#define CMN_LANE_CTRL1		0x0a4
#define CMN_TIMING_CTRL(n)	(0x0b4 + (n) * 4)
#define CMN_GLBL_HSTX_STR_CTRL_0 0x0ec
#define CMN_GLBL_RESCODE_TOP	0x0f4
#define CMN_GLBL_RESCODE_BOT	0x0f8
#define CMN_GLBL_LPTX_STR_CTRL	0x100
#define CMN_GLBL_PEMPH_CTRL_0	0x104
#define CMN_GLBL_STR_SWI_CAL	0x10c
#define CMN_VREG_CTRL_1		0x110
#define CMN_CTRL_4		0x114
#define CMN_GLBL_DIGTOP_SPARE4	0x128
#define CMN_PHY_STATUS		0x140
#define CMN_LANE_STATUS0	0x148
#define CMN_LANE_STATUS1	0x14c

#define CTRL_0_DIGTOP_PWRDN_B	BIT(6)
#define CTRL_0_PLL_SHUTDOWNB	BIT(5)
#define CLK_CFG1_CLK_EN		BIT(5)
#define CLK_CFG1_CLK_EN_SEL	BIT(4)
#define CLK_CFG1_BITCLK_SEL	GENMASK(3, 2)
#define CLK_CFG1_DSICLK_SEL	GENMASK(1, 0)

#define LN_CFG0(n)		((n) * 0x80 + 0x00)
#define LN_CFG1(n)		((n) * 0x80 + 0x04)
#define LN_CFG2(n)		((n) * 0x80 + 0x08)
#define LN_PIN_SWAP(n)		((n) * 0x80 + 0x10)
#define LN_LPRX_CTRL(n)		((n) * 0x80 + 0x14)
#define LN_TX_DCTRL(n)		((n) * 0x80 + 0x18)

#define PLL_ANALOG_CONTROLS_TWO		0x004
#define PLL_ANALOG_CONTROLS_THREE	0x010
#define PLL_ANALOG_CONTROLS_FIVE	0x018
#define PLL_DSM_DIVIDER			0x020
#define PLL_FEEDBACK_DIVIDER		0x024
#define PLL_SYSTEM_MUXES		0x028
#define PLL_CALIBRATION_SETTINGS	0x044
#define PLL_BAND_SEL_CAL_SETTINGS_THREE	0x068
#define PLL_FREQ_DETECT_SETTINGS_ONE	0x078
#define PLL_PFILT			0x090
#define PLL_IFILT			0x094
#define PLL_OUTDIV			0x0a8
#define PLL_CORE_OVERRIDE		0x0b8
#define PLL_CORE_INPUT_OVERRIDE		0x0bc
#define PLL_DIGITAL_TIMERS_TWO		0x0c8
#define PLL_DECIMAL_DIV_START_1		0x0e0
#define PLL_FRAC_DIV_START_LOW_1	0x0e4
#define PLL_FRAC_DIV_START_MID_1	0x0e8
#define PLL_FRAC_DIV_START_HIGH_1	0x0ec
#define PLL_OUTDIV_RATE			0x154
#define PLL_LOCKDET_RATE_1		0x158
#define PLL_PROP_GAIN_RATE_1		0x160
#define PLL_BAND_SEL_RATE_1		0x168
#define PLL_INT_GAIN_IFILT_BAND_1	0x170
#define PLL_FL_INT_GAIN_PFILT_BAND_1	0x178
#define PLL_LOCK_OVERRIDE		0x190
#define PLL_LOCK_DELAY			0x194
#define PLL_COMMON_STATUS_ONE		0x1b0
#define PLL_VCO_CONFIG_1		0x240
#define PLL_CLOCK_INVERTERS_1		0x248
#define PLL_CMODE_1			0x250
#define PLL_ANALOG_CONTROLS_FIVE_1	0x258
#define PLL_PERF_OPTIMIZE		0x260

/* DSI0 controller, 6G v2.5.1: registers are shifted by 4 bytes */
#define DSI_BASE		(0x0ae94000 + 4)
#define DSI_CTRL		0x000
#define DSI_STATUS0		0x004
#define DSI_FIFO_STATUS		0x008
#define DSI_VID_CFG0		0x00c
#define DSI_VID_CFG1		0x01c
#define DSI_ACTIVE_H		0x020
#define DSI_ACTIVE_V		0x024
#define DSI_TOTAL		0x028
#define DSI_ACTIVE_HSYNC	0x02c
#define DSI_ACTIVE_VSYNC_HPOS	0x030
#define DSI_ACTIVE_VSYNC_VPOS	0x034
#define DSI_CMD_DMA_CTRL	0x038
#define DSI_ACK_ERR_STATUS	0x064
#define DSI_TRIG_CTRL		0x080
#define DSI_LANE_STATUS		0x0a4
#define DSI_LANE_CTRL		0x0a8
#define DSI_LANE_SWAP_CTRL	0x0ac
#define DSI_DLN0_PHY_ERR	0x0b0
#define DSI_TIMEOUT_STATUS	0x0bc
#define DSI_CLKOUT_TIMING_CTRL	0x0c0
#define DSI_EOT_PACKET_CTRL	0x0c8
#define DSI_ERR_INT_MASK0	0x108
#define DSI_RESET		0x114
#define DSI_CLK_CTRL		0x118
#define DSI_CLK_STATUS		0x11c
#define DSI_TPG_CTRL		0x158
#define DSI_TPG_VIDEO_INIT_VAL	0x160
#define DSI_TPG_MAIN_CONTROL	0x198
#define DSI_TPG_VIDEO_CONFIG	0x1a0

#define DSI_CTRL_ENABLE		BIT(0)
#define DSI_CTRL_VID_MODE_EN	BIT(1)
#define DSI_CTRL_LANES(n)	(GENMASK((n) + 3, 4))
#define DSI_CTRL_CLK_EN		BIT(8)
#define VID_CFG0_DST_RGB888	(3 << 4)
#define VID_CFG0_BLLP_POWER_STOP BIT(12)
#define VID_CFG0_EOF_BLLP_POWER_STOP BIT(15)
#define VID_CFG0_PULSE_MODE_HSA_HE BIT(28)
#define CMD_DMA_FROM_FRAME_BUFFER BIT(28)
#define CMD_DMA_LOW_POWER	BIT(26)
#define TRIG_CTRL_DMA_SW	4
#define TRIG_CTRL_BLOCK_DMA	BIT(12)
#define TRIG_CTRL_TE		BIT(31)
#define LANE_CTRL_HS_REQ_SEL_PHY BIT(24)
#define LANE_CTRL_CLKLN_HS_FORCE BIT(28)
#define DSI_CLK_CTRL_ENABLE_CLKS 0x23f

/* DPU */
#define DPU_BASE		0x0ae01000
#define CTL0_BASE		(DPU_BASE + 0x15000)
#define CTL_TOP			0x014
#define CTL_FLUSH		0x018
#define CTL_INTF_ACTIVE		0x0f4
#define CTL_INTF_FLUSH		0x110
#define CTL_FLUSH_INTF		BIT(31)
#define CTL_GROUP_ID_NONE	(0xf << 28)

#define INTF1_BASE		(DPU_BASE + 0x35000)
#define INTF_TIMING_ENGINE_EN	0x000
#define INTF_CONFIG		0x004
#define INTF_HSYNC_CTL		0x008
#define INTF_VSYNC_PERIOD_F0	0x00c
#define INTF_VSYNC_PULSE_WIDTH_F0 0x014
#define INTF_DISPLAY_V_START_F0	0x01c
#define INTF_DISPLAY_V_END_F0	0x024
#define INTF_ACTIVE_V_START_F0	0x02c
#define INTF_ACTIVE_V_END_F0	0x034
#define INTF_DISPLAY_HCTL	0x03c
#define INTF_ACTIVE_HCTL	0x040
#define INTF_BORDER_COLOR	0x044
#define INTF_UNDERFLOW_COLOR	0x048
#define INTF_HSYNC_SKEW		0x04c
#define INTF_POLARITY_CTL	0x050
#define INTF_CONFIG2		0x060
#define INTF_DISPLAY_DATA_HCTL	0x064
#define INTF_ACTIVE_DATA_HCTL	0x068
#define INTF_PANEL_FORMAT	0x090
#define INTF_FRAME_LINE_COUNT_EN 0x0a8
#define INTF_FRAME_COUNT	0x0ac
#define INTF_LINE_COUNT		0x0b0
#define INTF_MUX		0x25c
#define INTF_STATUS		0x26c
#define INTF_CFG_ACTIVE_H_EN	BIT(29)
#define INTF_CFG_ACTIVE_V_EN	BIT(30)
#define INTF_CFG2_DATA_HCTL_EN	BIT(4)
#define INTF_PANEL_RGB888	0x213f

/* ADV7535 */
#define ADV_CEC_ADDR		0x3c
#define ADV_EDID_ADDR		0x3f
#define ADV_PACKET_ADDR		0x38

struct dphy_timing {
	s32 clk_zero, clk_trail, clk_prepare, hs_exit, hs_zero, hs_prepare;
	s32 hs_trail, hs_rqst, clk_post, clk_pre;
};

static int vrm_vote(const char *name, uint mv, uint mode)
{
	u32 addr;
	int ret;

	ret = cmd_db_find(name, &addr, NULL, NULL);
	if (ret) {
		mdss_log("  %s not in cmd-db\n", name);
		return ret;
	}
	ret = rsc_xfer(addr + VRM_VOLTAGE, mv, false, NULL);
	if (!ret)
		ret = rsc_xfer(addr + VRM_MODE, mode, false, NULL);
	if (!ret)
		ret = rsc_xfer(addr + VRM_ENABLE, 1, false, NULL);

	return ret;
}

static int rcg_set(const char *name, ulong rcgr, uint src, uint div2x)
{
	int ret;

	/* div2x is (2 * divider - 1), 0 to bypass */
	wr(rcgr + 4, src << RCG_SRC_SHIFT | div2x);
	wr(rcgr, rd(rcgr) | RCG_UPDATE);
	ret = poll(rcgr, RCG_UPDATE, 0, 1000, NULL);
	mdss_log("  %-10s cmd %08x cfg %08x%s\n", name, rd(rcgr), rd(rcgr + 4),
	       ret ? " UPDATE TIMEOUT (source not running?)" : "");

	return ret;
}

static int disp_pll0_enable(void)
{
	ulong p = DISPCC_PLL0;
	u32 val;
	int ret;

	if (rd(p + PLL_OPMODE) == PLL_RUN && (rd(p + PLL_MODE) & PLL_OUTCTRL)) {
		mdss_log("  PLL0 already running\n");
		return 0;
	}

	/* 19.2 MHz * (0x3a + 0x9800 / 2^16) = 1125 MHz */
	wr(p + PLL_L_VAL, 0x3a | PLL_CAL_L_VAL << 16);
	wr(p + PLL_ALPHA_VAL, 0x9800);
	wr(p + PLL_CONFIG_CTL, 0x20485699);
	wr(p + PLL_CONFIG_CTL_U, 0x00182261);
	wr(p + PLL_CONFIG_CTL_U1, 0x32aa299c);
	wr(p + PLL_USER_CTL, 0);
	wr(p + PLL_USER_CTL_U, 0x00400805);
	wr(p + PLL_TEST_CTL, 0);
	wr(p + PLL_TEST_CTL_U, 0);
	wr(p + PLL_TEST_CTL_U1, 0);
	wr(p + PLL_MODE, rd(p + PLL_MODE) & ~PLL_OUTCTRL);
	wr(p + PLL_OPMODE, 0);
	wr(p + PLL_MODE, rd(p + PLL_MODE) | PLL_RESET_N);

	wr(p + PLL_OPMODE, PLL_RUN);
	ret = poll(p + PLL_MODE, PLL_LOCK_DET, PLL_LOCK_DET, 1500, &val);
	mdss_log("  PLL0 mode %08x %s\n", val, ret ? "LOCK TIMEOUT" : "locked");
	if (ret)
		return ret;
	wr(p + PLL_USER_CTL, rd(p + PLL_USER_CTL) | PLL_OUT_MASK);
	wr(p + PLL_MODE, rd(p + PLL_MODE) | PLL_OUTCTRL);

	return 0;
}

static s32 s_div_round_up(s32 n, s32 d)
{
	return n >= 0 ? (n + d - 1) / d : (n - d + 1) / d;
}

static s32 linear_inter(s32 tmax, s32 tmin, s32 percent)
{
	return max_t(s32, 0, s_div_round_up((tmax - tmin) * percent, 100) +
		     tmin);
}

/* msm_dsi_dphy_timing_calc_v4() */
static void dphy_timing_calc(u64 bit_rate, struct dphy_timing *t)
{
	const s32 coeff = 1000;
	s32 ui, ui_x8, tmax, tmin, temp;

	ui = (s32)(1000000000ULL / (bit_rate / 1000));
	ui_x8 = ui << 3;

	tmin = max_t(s32, s_div_round_up(38 * coeff, ui_x8), 0);
	tmax = max_t(s32, (95 * coeff) / ui_x8, 0);
	t->clk_prepare = linear_inter(tmax, tmin, 50);

	temp = 300 * coeff - (t->clk_prepare << 3) * ui;
	tmin = s_div_round_up(temp, ui_x8) - 1;
	tmax = tmin > 255 ? 511 : 255;
	t->clk_zero = linear_inter(tmax, tmin, 2);

	tmin = DIV_ROUND_UP(60 * coeff + 3 * ui, ui_x8);
	temp = 105 * coeff + 12 * ui - 20 * coeff;
	tmax = (temp + 3 * ui) / ui_x8;
	t->clk_trail = linear_inter(tmax, tmin, 30);

	tmin = max_t(s32, s_div_round_up(40 * coeff + 4 * ui, ui_x8), 0);
	tmax = max_t(s32, (85 * coeff + 6 * ui) / ui_x8, 0);
	t->hs_prepare = linear_inter(tmax, tmin, 50);

	temp = 145 * coeff + 10 * ui - (t->hs_prepare << 3) * ui;
	tmin = s_div_round_up(temp, ui_x8) - 1;
	t->hs_zero = linear_inter(255, tmin, 10);

	tmin = DIV_ROUND_UP(60 * coeff + 4 * ui, ui_x8) - 1;
	temp = 105 * coeff + 12 * ui - 20 * coeff;
	tmax = (temp / ui_x8) - 1;
	t->hs_trail = linear_inter(tmax, tmin, 30);

	t->hs_rqst = s_div_round_up(50 * coeff - 8 * ui, ui_x8);

	tmin = DIV_ROUND_UP(100 * coeff, ui_x8) - 1;
	t->hs_exit = linear_inter(255, tmin, 10);

	temp = 60 * coeff + 52 * ui + (t->hs_trail + 1) * ui_x8;
	tmin = DIV_ROUND_UP(temp, 16 * ui) - 1;
	t->clk_post = linear_inter(255, tmin, 5);

	temp = 52 * coeff + (t->clk_prepare + t->clk_zero + 1) * ui_x8 +
		54 * coeff;
	tmin = DIV_ROUND_UP(temp, 16 * ui) - 1;
	t->clk_pre = DIV_ROUND_UP((255 - tmin) * 125, 10000) + tmin;
}

/* dsi_7nm_phy_enable() for a standalone D-PHY, v4.2 */
static int dsi_phy_enable(u64 bit_rate, const struct dphy_timing *t)
{
	bool slow = bit_rate <= 1500000000ULL;
	u32 val;
	int i, ret;

	ret = poll(PHY_CMN + CMN_PHY_STATUS, BIT(0), BIT(0), 1000, &val);
	mdss_log("  refgen ready: PHY_STATUS %08x%s\n", val,
	       ret ? " TIMEOUT" : "");
	if (ret)
		return ret;

	/* PLL system muxes as Linux leaves them after probing the PLL */
	wr(PHY_PLL + PLL_SYSTEM_MUXES, 0);
	wr(PHY_CMN + CMN_CTRL_0, CTRL_0_DIGTOP_PWRDN_B | CTRL_0_PLL_SHUTDOWNB);
	wr(PHY_CMN + CMN_PLL_CNTRL, 0);
	wr(PHY_CMN + CMN_RBUF_CTRL, 0);
	if ((rd(PHY_CMN + CMN_REVISION_ID0) & 0xf0) == 0x20)
		wr(PHY_CMN + CMN_CTRL_4, 0x04);
	wr(PHY_CMN + CMN_LANE_CFG0, 0x21);
	wr(PHY_CMN + CMN_LANE_CFG1, 0x84);

	wr(PHY_CMN + CMN_VREG_CTRL_0, slow ? 0x53 : 0x52);
	wr(PHY_CMN + CMN_VREG_CTRL_1, 0x5c);
	wr(PHY_CMN + CMN_CTRL_3, 0x00);
	wr(PHY_CMN + CMN_GLBL_STR_SWI_CAL, 0x00);
	wr(PHY_CMN + CMN_GLBL_HSTX_STR_CTRL_0, 0x88);
	wr(PHY_CMN + CMN_GLBL_PEMPH_CTRL_0, 0x00);
	wr(PHY_CMN + CMN_GLBL_RESCODE_TOP, slow ? 0x3c : 0x00);
	wr(PHY_CMN + CMN_GLBL_RESCODE_BOT, slow ? 0x38 : 0x39);
	wr(PHY_CMN + CMN_GLBL_LPTX_STR_CTRL, 0x55);

	wr(PHY_CMN + CMN_CTRL_0, 0x7f);
	wr(PHY_CMN + CMN_LANE_CTRL0, 0x1f);
	wr(PHY_CMN + CMN_CTRL_2, 0x40);

	/* Internal PLL as bit clock source */
	wr(PHY_CMN + CMN_CLK_CFG1, rd(PHY_CMN + CMN_CLK_CFG1) &
	   ~CLK_CFG1_BITCLK_SEL);

	wr(PHY_CMN + CMN_TIMING_CTRL(0), 0x00);
	wr(PHY_CMN + CMN_TIMING_CTRL(1), t->clk_zero);
	wr(PHY_CMN + CMN_TIMING_CTRL(2), t->clk_prepare);
	wr(PHY_CMN + CMN_TIMING_CTRL(3), t->clk_trail);
	wr(PHY_CMN + CMN_TIMING_CTRL(4), t->hs_exit);
	wr(PHY_CMN + CMN_TIMING_CTRL(5), t->hs_zero);
	wr(PHY_CMN + CMN_TIMING_CTRL(6), t->hs_prepare);
	wr(PHY_CMN + CMN_TIMING_CTRL(7), t->hs_trail);
	wr(PHY_CMN + CMN_TIMING_CTRL(8), t->hs_rqst);
	wr(PHY_CMN + CMN_TIMING_CTRL(9), 0x02);
	wr(PHY_CMN + CMN_TIMING_CTRL(10), 0x04);
	wr(PHY_CMN + CMN_TIMING_CTRL(11), 0x00);
	wr(PHY_CMN + CMN_TIMING_CTRL(12), t->clk_pre);
	wr(PHY_CMN + CMN_TIMING_CTRL(13), t->clk_post);

	/* Lane settings; LPRX/CDRX only on logical lane 0 */
	for (i = 0; i < 5; i++) {
		wr(PHY_LN + LN_LPRX_CTRL(i), 0);
		wr(PHY_LN + LN_PIN_SWAP(i), 0);
	}
	wr(PHY_LN + LN_LPRX_CTRL(0), 0x3);
	for (i = 0; i < 5; i++) {
		static const u8 tx_dctrl[] = { 0x40, 0x40, 0x40, 0x46, 0x41 };

		wr(PHY_LN + LN_CFG0(i), 0);
		wr(PHY_LN + LN_CFG1(i), 0);
		wr(PHY_LN + LN_CFG2(i), i == 4 ? 0x8a : 0x0a);
		wr(PHY_LN + LN_TX_DCTRL(i), tx_dctrl[i]);
	}

	return 0;
}

/*
 * dsi_pll_7nm_vco_set_rate() + dividers, v4.2, no SSC. The PLL bias is left
 * as dsi_phy_enable() set it: Linux refcounts it and, once the PHY is on,
 * set_rate/prepare never toggle it.
 */
static void dsi_pll_set_rate(u64 vco, uint bitdiv, uint pixmux, uint pixdiv)
{
	u64 mult = vco * (1 << 18);
	u32 dec, frac, inverters;

	dec = mult / (VCO_REF_HZ * 2) >> 18;
	frac = (mult / (VCO_REF_HZ * 2)) & ((1 << 18) - 1);
	if (vco <= 1000000000ULL)
		inverters = 0xa0;
	else if (vco <= 2500000000ULL)
		inverters = 0x20;
	else if (vco <= 3500000000ULL)
		inverters = 0x00;
	else
		inverters = 0x40;
	mdss_log("  VCO %llu Hz: dec %u frac %u, bit div %u, pixel %s/%u\n",
	       vco, dec, frac, bitdiv, pixmux ? "bit/2" : "bit", pixdiv);

	wr(PHY_PLL + PLL_CORE_INPUT_OVERRIDE, 0x12);
	wr(PHY_PLL + PLL_DECIMAL_DIV_START_1, dec);
	wr(PHY_PLL + PLL_FRAC_DIV_START_LOW_1, frac & 0xff);
	wr(PHY_PLL + PLL_FRAC_DIV_START_MID_1, (frac >> 8) & 0xff);
	wr(PHY_PLL + PLL_FRAC_DIV_START_HIGH_1, (frac >> 16) & 0x3);
	wr(PHY_PLL + PLL_LOCKDET_RATE_1, 0x40);
	wr(PHY_PLL + PLL_LOCK_DELAY, 0x06);
	wr(PHY_PLL + PLL_CMODE_1, 0x10);
	wr(PHY_PLL + PLL_CLOCK_INVERTERS_1, inverters);

	wr(PHY_PLL + PLL_ANALOG_CONTROLS_FIVE_1,
	   vco >= 3100000000ULL ? 0x03 : 0x01);
	wr(PHY_PLL + PLL_VCO_CONFIG_1, vco < 1520000000ULL ? 0x08 :
	   vco >= 2990000000ULL ? 0x01 : 0x00);
	wr(PHY_PLL + PLL_ANALOG_CONTROLS_FIVE, 0x01);
	wr(PHY_PLL + PLL_ANALOG_CONTROLS_TWO, 0x03);
	wr(PHY_PLL + PLL_ANALOG_CONTROLS_THREE, 0x00);
	wr(PHY_PLL + PLL_DSM_DIVIDER, 0x00);
	wr(PHY_PLL + PLL_FEEDBACK_DIVIDER, 0x4e);
	wr(PHY_PLL + PLL_CALIBRATION_SETTINGS, 0x40);
	wr(PHY_PLL + PLL_BAND_SEL_CAL_SETTINGS_THREE, 0xba);
	wr(PHY_PLL + PLL_FREQ_DETECT_SETTINGS_ONE, 0x0c);
	wr(PHY_PLL + PLL_OUTDIV, 0x00);
	wr(PHY_PLL + PLL_CORE_OVERRIDE, 0x00);
	wr(PHY_PLL + PLL_DIGITAL_TIMERS_TWO, 0x08);
	wr(PHY_PLL + PLL_PROP_GAIN_RATE_1, 0x0a);
	wr(PHY_PLL + PLL_BAND_SEL_RATE_1, 0xc0);
	wr(PHY_PLL + PLL_INT_GAIN_IFILT_BAND_1, 0x84);
	wr(PHY_PLL + PLL_INT_GAIN_IFILT_BAND_1, 0x82);
	wr(PHY_PLL + PLL_FL_INT_GAIN_PFILT_BAND_1, 0x4c);
	wr(PHY_PLL + PLL_LOCK_OVERRIDE, 0x80);
	wr(PHY_PLL + PLL_PFILT, 0x29);
	wr(PHY_PLL + PLL_PFILT, 0x2f);
	wr(PHY_PLL + PLL_IFILT, 0x2a);
	wr(PHY_PLL + PLL_IFILT, 0x3f);
	wr(PHY_PLL + PLL_PERF_OPTIMIZE, 0x22);

	/* Post dividers: out_div 1, bit clock and pixel clock dividers */
	wr(PHY_PLL + PLL_OUTDIV_RATE, rd(PHY_PLL + PLL_OUTDIV_RATE) & ~0x3);
	wr(PHY_CMN + CMN_CLK_CFG0, bitdiv | pixdiv << 4);
	wr(PHY_CMN + CMN_CLK_CFG1, (rd(PHY_CMN + CMN_CLK_CFG1) &
	   ~CLK_CFG1_DSICLK_SEL) | pixmux);
}

/* dsi_pll_7nm_vco_prepare() */
static int dsi_pll_start(void)
{
	u32 val;
	int ret;

	wr(PHY_CMN + CMN_PLL_CNTRL, BIT(0));
	ret = poll(PHY_PLL + PLL_COMMON_STATUS_ONE, BIT(0), BIT(0), 5000,
		   &val);
	mdss_log("  DSI PLL status %08x %s\n", val, ret ? "LOCK FAILED" :
	       "locked");
	if (ret)
		return ret;

	/* Reset the PHY digital domain, then enable the global clock */
	wr(PHY_CMN + CMN_GLBL_DIGTOP_SPARE4, BIT(0));
	wr(PHY_CMN + CMN_GLBL_DIGTOP_SPARE4, 0);
	wr(PHY_CMN + CMN_CTRL_3, 0x04);
	wr(PHY_CMN + CMN_CLK_CFG1, rd(PHY_CMN + CMN_CLK_CFG1) |
	   CLK_CFG1_CLK_EN | CLK_CFG1_CLK_EN_SEL);
	wr(PHY_CMN + CMN_RBUF_CTRL, 1);

	return 0;
}

static void dsi_wr(uint reg, u32 val)
{
	wr(DSI_BASE + reg, val);
}

static u32 dsi_rd(uint reg)
{
	return rd(DSI_BASE + reg);
}

/* dsi_timing_setup(), dsi_sw_reset(), dsi_ctrl_enable(), host enable */
static void dsi_ctrl_start(const struct mdss_mode *m,
			   const struct dphy_timing *t, bool tpg)
{
	uint h_total = m->hact + m->hfp + m->hsync + m->hbp;
	uint v_total = m->vact + m->vfp + m->vsync + m->vbp;
	uint ha_start = h_total - (m->hact + m->hfp);
	uint va_start = v_total - (m->vact + m->vfp);

	dsi_wr(DSI_ACTIVE_H, ha_start | (ha_start + m->hact) << 16);
	dsi_wr(DSI_ACTIVE_V, va_start | (va_start + m->vact) << 16);
	dsi_wr(DSI_TOTAL, (h_total - 1) | (v_total - 1) << 16);
	dsi_wr(DSI_ACTIVE_HSYNC, m->hsync << 16);
	dsi_wr(DSI_ACTIVE_VSYNC_HPOS, 0);
	dsi_wr(DSI_ACTIVE_VSYNC_VPOS, m->vsync << 16);

	dsi_wr(DSI_CLK_CTRL, DSI_CLK_CTRL_ENABLE_CLKS);
	dsi_wr(DSI_RESET, 1);
	mdelay(20);
	dsi_wr(DSI_RESET, 0);

	/* ADV7533: video, sync pulses, HSE, no EoT packet */
	dsi_wr(DSI_VID_CFG0, VID_CFG0_PULSE_MODE_HSA_HE |
	       VID_CFG0_EOF_BLLP_POWER_STOP | VID_CFG0_BLLP_POWER_STOP |
	       VID_CFG0_DST_RGB888);
	dsi_wr(DSI_VID_CFG1, 0);
	dsi_wr(DSI_CMD_DMA_CTRL, CMD_DMA_FROM_FRAME_BUFFER | CMD_DMA_LOW_POWER);
	dsi_wr(DSI_TRIG_CTRL, TRIG_CTRL_TE | TRIG_CTRL_DMA_SW |
	       TRIG_CTRL_BLOCK_DMA);
	dsi_wr(DSI_CLKOUT_TIMING_CTRL, (t->clk_pre & 0x3f) |
	       (t->clk_post & 0x3f) << 8);
	dsi_wr(DSI_EOT_PACKET_CTRL, 0);
	dsi_wr(DSI_ERR_INT_MASK0, 0x13ff3fe0);
	dsi_wr(DSI_CLK_CTRL, DSI_CLK_CTRL_ENABLE_CLKS);
	dsi_wr(DSI_LANE_SWAP_CTRL, 0);

	/* Continuous HS clock */
	wr(PHY_CMN + CMN_LANE_CTRL1, rd(PHY_CMN + CMN_LANE_CTRL1) |
	   BIT(5) | BIT(6));
	dsi_wr(DSI_LANE_CTRL, (dsi_rd(DSI_LANE_CTRL) &
	       ~LANE_CTRL_HS_REQ_SEL_PHY) | LANE_CTRL_CLKLN_HS_FORCE);

	dsi_wr(DSI_CTRL, DSI_CTRL_CLK_EN | DSI_CTRL_LANES(DSI_LANES) |
	       DSI_CTRL_ENABLE);

	if (tpg) {
		/* Checkered rectangles, 24 bpp RGB */
		dsi_wr(DSI_TPG_VIDEO_INIT_VAL, 0xff);
		dsi_wr(DSI_TPG_MAIN_CONTROL, BIT(8));
		dsi_wr(DSI_TPG_VIDEO_CONFIG, 1 | BIT(2));
		dsi_wr(DSI_TPG_CTRL, 3 << 4 | BIT(0));
	}

	dsi_wr(DSI_CTRL, dsi_rd(DSI_CTRL) | DSI_CTRL_VID_MODE_EN |
	       DSI_CTRL_ENABLE);
}

/* Stage 3: DMA0 pipe -> LM0 -> PP0 -> INTF1, linear XRGB8888 */
#define SSPP_DMA0_BASE		(DPU_BASE + 0x24000)
#define SSPP_SRC_SIZE		0x000
#define SSPP_SRC_XY		0x008
#define SSPP_OUT_SIZE		0x00c
#define SSPP_OUT_XY		0x010
#define SSPP_SRC0_ADDR		0x014
#define SSPP_SRC_YSTRIDE0	0x024
#define SSPP_SRC_YSTRIDE1	0x028
#define SSPP_SRC_FORMAT		0x030
#define SSPP_SRC_UNPACK_PATTERN	0x034
#define SSPP_SRC_OP_MODE	0x038
#define SSPP_UBWC_ERROR_STATUS	0x138
#define SSPP_QOS_CTRL		0x06c
#define SSPP_DECIMATION_CONFIG	0x0b4
#define SSPP_SW_PIX_EXT_C0_LR	0x100
#define SSPP_SW_PIX_EXT_C0_TB	0x104
#define SSPP_SW_PIX_EXT_C0_REQ	0x108
#define SSPP_SW_PIX_EXT_C12_LR	0x110
#define SSPP_SW_PIX_EXT_C12_TB	0x114
#define SSPP_SW_PIX_EXT_C12_REQ	0x118
#define SSPP_SW_PIX_EXT_C3_LR	0x120
#define SSPP_SW_PIX_EXT_C3_TB	0x124
#define SSPP_SW_PIX_EXT_C3_REQ	0x128
#define SSPP_TRAFFIC_SHAPER	0x130
#define SSPP_CDP_CNTL		0x134
#define SSPP_MULTIRECT_OPMODE	0x170
#define SSPP_OP_PE_OVERRIDE	BIT(31)
#define SSPP_OP_FLIPS_BWC	(BIT(13) | BIT(14) | BIT(0))
/* XRGB8888: 8 bpc, 4 bytes, 4 unpacked elements B,G,R,A, tight */
#define SSPP_FORMAT_XRGB8888	0x000236ff
#define SSPP_UNPACK_XRGB8888	0x03020001
#define CDP_ENABLE_PRELOAD_64	(BIT(0) | BIT(3))

#define LM0_BASE		(DPU_BASE + 0x44000)
#define LM_OP_MODE		0x000
#define LM_OUT_SIZE		0x004
#define LM_BLEND_STAGE0		0x020	/* first stage above the base */
#define LM_BLEND0_OP		0x000
#define LM_BLEND0_CONST_ALPHA	0x004

#define CTL_LAYER_LM0		0x000
#define CTL_LAYER_EXT_LM0	0x040
#define CTL_LAYER_EXT2_LM0	0x070
#define CTL_LAYER_EXT3_LM0	0x0a0
#define CTL_FETCH_PIPE_ACTIVE	0x0fc
#define CTL_MIXER_BORDER_OUT	BIT(24)
#define CTL_LAYER_DMA0_STAGE0	(2 << 18)
#define CTL_FLUSH_DMA0		BIT(11)
#define CTL_FLUSH_LM0		BIT(6)
#define CTL_FLUSH_CTL		BIT(17)
#define CTL_FETCH_DMA0		BIT(0)

#define SMMU_CB_FSR		0x058
#define SMMU_CB_FAR		0x060

/* Framebuffer being scanned out, 0 if none */
static ulong fb_addr;

int mdss_fb_alloc(const struct mdss_mode *m, ulong *fbp)
{
	struct efi_boot_services *boot = efi_get_boot();
	uint size = m->hact * m->vact * 4;
	efi_physical_addr_t addr = 0xffffffff;
	efi_status_t status;
	static ulong fb;

	if (!fb) {
		/*
		 * If scanout continues into the OS, the OS must never reuse
		 * this memory; otherwise boot-services memory is enough since
		 * scanout stops at ExitBootServices()
		 */
		status = boot->allocate_pages(EFI_ALLOCATE_MAX_ADDRESS,
				IS_ENABLED(CONFIG_VIDEO_QCOM_MDSS_EFI_KEEP_SCANOUT) ?
				EFI_RESERVED_MEMORY_TYPE : EFI_BOOT_SERVICES_DATA,
				DIV_ROUND_UP(size, EFI_PAGE_SIZE), &addr);
		if (status) {
			mdss_log("  allocate_pages failed: %lx\n", status);
			return -ENOMEM;
		}
		fb = addr;
	}
	*fbp = fb;

	return 0;
}

/* Colour bars, a grey ramp in the bottom quarter, white frame */
static void __maybe_unused fb_draw_test(const struct mdss_mode *m, ulong addr)
{
	uint size = m->hact * m->vact * 4;
	u32 *fb = (u32 *)addr, colour;
	uint x, y;
	static const u32 bars[] = {
		0xffffff, 0xffff00, 0x00ffff, 0x00ff00,
		0xff00ff, 0xff0000, 0x0000ff, 0x000000,
	};

	for (y = 0; y < m->vact; y++) {
		for (x = 0; x < m->hact; x++) {
			if (y < 4 || y >= m->vact - 4 || x < 4 ||
			    x >= m->hact - 4) {
				colour = 0xffffff;
			} else if (y < m->vact * 3 / 4) {
				colour = bars[x * 8 / m->hact];
			} else {
				colour = x * 256 / m->hact;
				colour |= colour << 8 | colour << 16;
			}
			fb[y * m->hact + x] = colour;
		}
	}
	flush_dcache_range(addr, addr + size);
	mdss_log("  framebuffer at %08lx, %u bytes\n", addr, size);
}

/* dpu_plane / dpu_crtc programming for one full-screen opaque plane */
static void pipe_setup(const struct mdss_mode *m)
{
	ulong s = SSPP_DMA0_BASE, l = LM0_BASE;
	u32 size = m->vact << 16 | m->hact;

	wr(s + SSPP_MULTIRECT_OPMODE, 0);
	wr(s + SSPP_SRC_SIZE, size);
	wr(s + SSPP_SRC_XY, 0);
	wr(s + SSPP_OUT_SIZE, size);
	wr(s + SSPP_OUT_XY, 0);
	wr(s + SSPP_SRC0_ADDR, fb_addr);
	wr(s + SSPP_SRC_YSTRIDE0, m->hact * 4);
	wr(s + SSPP_SRC_YSTRIDE1, 0);
	wr(s + SSPP_SRC_FORMAT, SSPP_FORMAT_XRGB8888);
	wr(s + SSPP_SRC_UNPACK_PATTERN, SSPP_UNPACK_XRGB8888);
	wr(s + SSPP_SRC_OP_MODE, (rd(s + SSPP_SRC_OP_MODE) &
	   ~SSPP_OP_FLIPS_BWC) | SSPP_OP_PE_OVERRIDE);
	wr(s + SSPP_UBWC_ERROR_STATUS, BIT(31));
	wr(s + SSPP_DECIMATION_CONFIG, 0);
	wr(s + SSPP_TRAFFIC_SHAPER, 0);
	wr(s + SSPP_QOS_CTRL, 0);
	wr(s + SSPP_CDP_CNTL, CDP_ENABLE_PRELOAD_64);

	/* No scaler: request exactly the source rectangle for each colour */
	wr(s + SSPP_SW_PIX_EXT_C0_LR, 0);
	wr(s + SSPP_SW_PIX_EXT_C0_TB, 0);
	wr(s + SSPP_SW_PIX_EXT_C0_REQ, size);
	wr(s + SSPP_SW_PIX_EXT_C12_LR, 0);
	wr(s + SSPP_SW_PIX_EXT_C12_TB, 0);
	wr(s + SSPP_SW_PIX_EXT_C12_REQ, size);
	wr(s + SSPP_SW_PIX_EXT_C3_LR, 0);
	wr(s + SSPP_SW_PIX_EXT_C3_TB, 0);
	wr(s + SSPP_SW_PIX_EXT_C3_REQ, size);

	/* LM0: full output, plane at stage 0 fully opaque */
	wr(l + LM_OUT_SIZE, size);
	wr(l + LM_OP_MODE, 0);
	wr(l + LM_BLEND_STAGE0 + LM_BLEND0_CONST_ALPHA, 0xff << 16);
	wr(l + LM_BLEND_STAGE0 + LM_BLEND0_OP, 0);

	wr(CTL0_BASE + CTL_LAYER_LM0, CTL_MIXER_BORDER_OUT |
	   CTL_LAYER_DMA0_STAGE0);
	wr(CTL0_BASE + CTL_LAYER_EXT_LM0, 0);
	wr(CTL0_BASE + CTL_LAYER_EXT2_LM0, 0);
	wr(CTL0_BASE + CTL_LAYER_EXT3_LM0, 0);
	wr(CTL0_BASE + CTL_FETCH_PIPE_ACTIVE, CTL_FETCH_DMA0);
}

/*
 * dpu_hw_intf_setup_timing_engine(), then start. Without a framebuffer, a
 * border window is used and INTF1 has no pingpong source.
 */
static void intf_start(const struct mdss_mode *m, bool use_fb)
{
	uint xres = use_fb ? m->hact : m->hact / 2;
	uint yres = use_fb ? m->vact : m->vact / 2;
	u32 hsync_period = m->hsync + m->hbp + m->hact + m->hfp;
	u32 vsync_period = m->vsync + m->vbp + m->vact + m->vfp;
	u32 display_v_start = (m->vsync + m->vbp) * hsync_period;
	u32 display_v_end = (vsync_period - m->vfp) * hsync_period - 1;
	u32 hsync_start_x = m->hbp + m->hsync;
	u32 hsync_end_x = hsync_period - m->hfp - 1;
	u32 active_h_end = hsync_start_x + xres - 1;
	u32 active_v_end = display_v_start + yres * hsync_period - 1;
	ulong i = INTF1_BASE;

	wr(i + INTF_TIMING_ENGINE_EN, 0);
	wr(i + INTF_HSYNC_CTL, hsync_period << 16 | m->hsync);
	wr(i + INTF_VSYNC_PERIOD_F0, vsync_period * hsync_period);
	wr(i + INTF_VSYNC_PULSE_WIDTH_F0, m->vsync * hsync_period);
	wr(i + INTF_DISPLAY_HCTL, hsync_end_x << 16 | hsync_start_x);
	wr(i + INTF_DISPLAY_V_START_F0, display_v_start);
	wr(i + INTF_DISPLAY_V_END_F0, display_v_end);
	if (use_fb) {
		wr(i + INTF_ACTIVE_HCTL, 0);
		wr(i + INTF_ACTIVE_V_START_F0, 0);
		wr(i + INTF_ACTIVE_V_END_F0, 0);
	} else {
		wr(i + INTF_ACTIVE_HCTL, active_h_end << 16 | hsync_start_x);
		wr(i + INTF_ACTIVE_V_START_F0, display_v_start);
		wr(i + INTF_ACTIVE_V_END_F0, active_v_end);
	}
	wr(i + INTF_BORDER_COLOR, use_fb ? 0 : 0x0000ff);
	wr(i + INTF_UNDERFLOW_COLOR, use_fb ? 0xff : 0xff0000);
	wr(i + INTF_HSYNC_SKEW, 0);
	wr(i + INTF_POLARITY_CTL, 0);
	wr(i + INTF_FRAME_LINE_COUNT_EN, 0x3);
	/* No programmable fetch: vbp + vsw covers the worst case 24 lines */
	wr(i + INTF_CONFIG, (rd(i + INTF_CONFIG) & ~(BIT(31) |
	   INTF_CFG_ACTIVE_H_EN | INTF_CFG_ACTIVE_V_EN)) |
	   (use_fb ? 0 : INTF_CFG_ACTIVE_H_EN | INTF_CFG_ACTIVE_V_EN));
	wr(i + INTF_PANEL_FORMAT, INTF_PANEL_RGB888);
	wr(i + INTF_CONFIG2, INTF_CFG2_DATA_HCTL_EN);
	wr(i + INTF_DISPLAY_DATA_HCTL,
	   (hsync_start_x + m->hact - 1) << 16 | hsync_start_x);
	wr(i + INTF_ACTIVE_DATA_HCTL, 0);
	/* PP0 feeds this interface, or nothing (0xf) */
	wr(i + INTF_MUX, (rd(i + INTF_MUX) & ~0xf) | (use_fb ? 0 : 0xf));

	/* CTL0 routes INTF1 in video mode; flush before starting */
	wr(CTL0_BASE + CTL_TOP, CTL_GROUP_ID_NONE);
	wr(CTL0_BASE + CTL_INTF_ACTIVE, BIT(1));
	if (use_fb)
		pipe_setup(m);
	wr(CTL0_BASE + CTL_INTF_FLUSH, BIT(1));
	wr(CTL0_BASE + CTL_FLUSH, CTL_FLUSH_INTF | (use_fb ? CTL_FLUSH_DMA0 |
	   CTL_FLUSH_LM0 | CTL_FLUSH_CTL : 0));
	udelay(100);

	wr(i + INTF_TIMING_ENGINE_EN, 1);
}

struct adv_reg {
	u8 addr;
	u8 reg;
	u8 val;
};

static int adv_write_seq(const struct adv_reg *seq, uint count)
{
	uint i;
	int ret = 0;

	for (i = 0; i < count; i++) {
		if (bb_write_reg(seq[i].addr, seq[i].reg, seq[i].val)) {
			mdss_log("  write %02x:%02x=%02x failed\n", seq[i].addr,
			       seq[i].reg, seq[i].val);
			ret = -EREMOTEIO;
		}
	}

	return ret;
}

static int adv_update(u8 addr, u8 reg, u8 mask, u8 val)
{
	u8 old;
	int ret;

	ret = bb_read_reg(addr, reg, &old);
	if (!ret)
		ret = bb_write_reg(addr, reg, (old & ~mask) | val);
	if (ret)
		mdss_log("  update %02x:%02x failed\n", addr, reg);

	return ret;
}

/* adv7511 probe + power on + mode set for an ADV7535 with 4 DSI lanes */
static int adv7535_start(const struct mdss_mode *m)
{
	uint htotal = m->hact + m->hfp + m->hsync + m->hbp;
	uint vtotal = m->vact + m->vfp + m->vsync + m->vbp;
	const u8 a = ADV7535_ADDR, c = ADV_CEC_ADDR;
	const struct adv_reg setup[] = {
		/* ancillary map addresses */
		{ a, 0xe1, ADV_CEC_ADDR << 1 },
		{ a, 0x43, ADV_EDID_ADDR << 1 },
		{ a, 0x45, ADV_PACKET_ADDR << 1 },
	};
	const struct adv_reg fixed[] = {
		{ a, 0x16, 0x20 }, { a, 0x9a, 0xe0 }, { a, 0xba, 0x70 },
		{ a, 0xde, 0x82 }, { a, 0xe4, 0x40 }, { a, 0xe5, 0x80 },
	};
	const struct adv_reg cec_fixed[] = {
		{ c, 0x15, 0xd0 }, { c, 0x17, 0xd0 }, { c, 0x24, 0x20 },
		{ c, 0x57, 0x11 }, { c, 0x05, 0xc8 },
	};
	const struct adv_reg dsi_on[] = {
		{ c, 0x1c, DSI_LANES << 4 },
		/* reset the internal timing generator */
		{ c, 0x27, 0xcb }, { c, 0x27, 0x8b }, { c, 0x27, 0xcb },
		{ c, 0x03, 0x89 },	/* enable HDMI */
		{ c, 0x55, 0x00 },	/* no test mode */
	};
	const struct adv_reg timing[] = {
		{ c, 0x16, 3 << 3 },	/* pixel clock divider, 4 lanes */
		{ c, 0x28, htotal >> 4 }, { c, 0x29, (htotal << 4) & 0xff },
		{ c, 0x2a, m->hsync >> 4 }, { c, 0x2b, (m->hsync << 4) & 0xff },
		{ c, 0x2c, m->hfp >> 4 }, { c, 0x2d, (m->hfp << 4) & 0xff },
		{ c, 0x2e, m->hbp >> 4 }, { c, 0x2f, (m->hbp << 4) & 0xff },
		{ c, 0x30, vtotal >> 4 }, { c, 0x31, (vtotal << 4) & 0xff },
		{ c, 0x32, m->vsync >> 4 }, { c, 0x33, (m->vsync << 4) & 0xff },
		{ c, 0x34, m->vfp >> 4 }, { c, 0x35, (m->vfp << 4) & 0xff },
		{ c, 0x36, m->vbp >> 4 }, { c, 0x37, (m->vbp << 4) & 0xff },
	};
	u8 rev;
	int ret;

	bb_init();
	ret = bb_read_reg(a, 0x00, &rev);
	mdss_log("  revision %02x%s\n", rev, ret ? " (no ack)" : "");
	if (ret)
		return ret;

	/*
	 * The bridge is not reset with the SoC and may hold the state the
	 * OS left. Powering it down resets most registers, so cycle it.
	 */
	ret = adv_update(a, 0x41, BIT(6), BIT(6));
	mdelay(5);
	ret |= adv_write_seq(setup, ARRAY_SIZE(setup));
	/* power up, HPD override (ADV7535) */
	ret |= adv_update(a, 0x41, BIT(6), 0);
	ret |= adv_update(a, 0xd6, BIT(6), BIT(6));
	ret |= adv_write_seq(fixed, ARRAY_SIZE(fixed));
	ret |= adv_write_seq(dsi_on, ARRAY_SIZE(dsi_on));
	ret |= adv_write_seq(cec_fixed, ARRAY_SIZE(cec_fixed));
	/* RGB in, no CSC, DVI mode (no infoframes needed) */
	ret |= adv_update(a, 0x18, BIT(7), 0);
	ret |= adv_update(a, 0x16, 0x81, 0);
	ret |= adv_update(a, 0xaf, BIT(1), 0);
	/* mode set: no low refresh rate, sync polarity pass-through */
	ret |= adv_update(a, 0x4a, 0x0c, 0);
	ret |= adv_update(a, 0x17, 0x60, 0);
	ret |= adv_write_seq(timing, ARRAY_SIZE(timing));

	return ret ? -EREMOTEIO : 0;
}

static void hdmi_status(void)
{
	static const u8 adv_regs[] = { 0x41, 0x42, 0x3d, 0x3e, 0x96, 0xd6 };
	u32 f0, f1;
	uint i;

	mdss_log("Status:\n");
	mdss_log("  DSI PLL status %08x, PHY lane status %08x %08x\n",
	       rd(PHY_PLL + PLL_COMMON_STATUS_ONE),
	       rd(PHY_CMN + CMN_LANE_STATUS0), rd(PHY_CMN + CMN_LANE_STATUS1));
	mdss_log("  DSI ctrl %08x status0 %08x fifo %08x lanes %08x clk %08x\n",
	       dsi_rd(DSI_CTRL), dsi_rd(DSI_STATUS0), dsi_rd(DSI_FIFO_STATUS),
	       dsi_rd(DSI_LANE_STATUS), dsi_rd(DSI_CLK_STATUS));
	mdss_log("  DSI errors: ack %08x timeout %08x dln0 %08x\n",
	       dsi_rd(DSI_ACK_ERR_STATUS), dsi_rd(DSI_TIMEOUT_STATUS),
	       dsi_rd(DSI_DLN0_PHY_ERR));
	f0 = rd(INTF1_BASE + INTF_FRAME_COUNT);
	mdelay(100);
	f1 = rd(INTF1_BASE + INTF_FRAME_COUNT);
	mdss_log("  INTF1 frames %u -> %u in 100ms (expect ~6), line %u, status %08x\n",
	       f0, f1, rd(INTF1_BASE + INTF_LINE_COUNT),
	       rd(INTF1_BASE + INTF_STATUS));
	mdss_log("  CTL0 flush pending %08x, DMA0 addr %08x status %08x\n",
	       rd(CTL0_BASE + CTL_FLUSH), rd(SSPP_DMA0_BASE + SSPP_SRC0_ADDR),
	       rd(SSPP_DMA0_BASE + 0x070));
	if (smmu_cb_base)
		mdss_log("  SMMU CB FSR %08x FAR %08x%08x (FSR 0 = no faults)\n",
		       rd(smmu_cb_base + SMMU_CB_FSR),
		       rd(smmu_cb_base + SMMU_CB_FAR + 4),
		       rd(smmu_cb_base + SMMU_CB_FAR));

	mdss_log("  ADV7535:");
	for (i = 0; i < ARRAY_SIZE(adv_regs); i++) {
		u8 val = 0;

		if (bb_read_reg(ADV7535_ADDR, adv_regs[i], &val))
			mdss_log(" %02x=??", adv_regs[i]);
		else
			mdss_log(" %02x=%02x", adv_regs[i], val);
	}
	mdss_log("\n  (42 bit6 = HPD, bit5 = monitor sense; 3e[7:2] = VIC seen, 4 = 720p60)\n");
}

int mdss_display_start(const struct mdss_mode *m, ulong fb, bool dsi_tpg)
{
	u64 bit_rate = (u64)m->pclk_khz * 1000 * DSI_BPP / DSI_LANES;
	u64 pclk = (u64)m->pclk_khz * 1000;
	uint bitdiv, pixmux, pixdiv;
	struct dphy_timing t;
	u64 vco;
	int i, ret;

	/* Pick the PLL dividers: VCO >= 600 MHz, pixel = bit (/2) / div */
	for (bitdiv = 1; bit_rate * bitdiv < VCO_MIN_HZ; bitdiv++)
		;
	vco = bit_rate * bitdiv;
	pixmux = !(bit_rate % (2 * pclk));
	pixdiv = pixmux ? bit_rate / (2 * pclk) : bit_rate / pclk;

	dphy_timing_calc(bit_rate, &t);
	mdss_log("Mode %ux%u, pclk %u kHz, %u lanes at %llu bps\n", m->hact,
		 m->vact, m->pclk_khz, DSI_LANES, bit_rate);
	mdss_log("  D-PHY: clk zero %d trail %d prep %d, hs exit %d zero %d prep %d trail %d rqst %d, pre %d post %d\n",
		 t.clk_zero, t.clk_trail, t.clk_prepare, t.hs_exit, t.hs_zero,
		 t.hs_prepare, t.hs_trail, t.hs_rqst, t.clk_pre, t.clk_post);

	mdss_log("Step 1: power, bandwidth, supplies\n");
	ret = do_power(false);
	if (!ret)
		ret = do_bw();
	if (ret)
		return ret;
	mdss_log("  vreg_l5a %u mV HPM\n", L5A_MV);
	ret = vrm_vote("ldoa5", L5A_MV, LDO_MODE_HPM);
	if (ret)
		return ret;
	wr(REFGEN_PWRDWN_CTRL5, rd(REFGEN_PWRDWN_CTRL5) | BIT(0));
	udelay(10);
	mdss_log("  refgen %08x\n", rd(REFGEN_PWRDWN_CTRL5));
	wr(DISPCC_XO_CBCR, rd(DISPCC_XO_CBCR) | CBCR_CLK_ENABLE);
	wr(DISPCC_SLEEP_CBCR, rd(DISPCC_SLEEP_CBCR) | CBCR_CLK_ENABLE);

	mdss_log("Step 2: MDP clock from dispcc PLL0 (375 MHz)\n");
	if (!disp_pll0_enable())
		rcg_set("MDP", DISPCC_MDP_RCGR, SRC_DISP_PLL0, 2 * 3 - 1);
	else
		mdss_log("  staying on XO\n");

	mdss_log("Step 3: DSI PHY\n");
	ret = dsi_phy_enable(bit_rate, &t);
	if (ret)
		return ret;

	mdss_log("Step 4: DSI PLL\n");
	dsi_pll_set_rate(vco, bitdiv, pixmux, pixdiv);
	ret = dsi_pll_start();
	if (ret)
		return ret;

	mdss_log("Step 5: DSI link clocks\n");
	ret = rcg_set("BYTE0", DISPCC_BYTE0_RCGR, SRC_DSI0_BYTE, 0);
	ret |= rcg_set("PCLK0", DISPCC_PCLK0_RCGR, SRC_DSI0_PIXEL, 0);
	ret |= rcg_set("ESC0", DISPCC_ESC0_RCGR, SRC_XO, 0);
	wr(DISPCC_BYTE0_DIV, 1);	/* byte_intf = byte / 2 */
	ret |= branch_enable("ESC0", DISPCC_MDSS_ESC0_CBCR);
	ret |= branch_enable("BYTE0", DISPCC_MDSS_BYTE0_CBCR);
	ret |= branch_enable("PCLK0", DISPCC_PCLK0_CBCR);
	ret |= branch_enable("BYTE0_INTF", DISPCC_BYTE0_INTF_CBCR);
	if (ret)
		return -EIO;

	mdss_log("Step 6: DSI controller%s\n",
		 dsi_tpg ? " with test pattern" : "");
	dsi_ctrl_start(m, &t, dsi_tpg);

	if (fb) {
		mdss_log("Step 7a: SMMU identity mapping for %08lx\n", fb);
		ret = do_smmu_map();
		if (ret)
			return ret;
		fb_addr = fb;
	}

	mdss_log("Step 7: DPU %sINTF1 timing engine\n",
		 fb ? "DMA0 -> LM0 -> PP0 -> " : "");
	intf_start(m, fb != 0);

	mdss_log("Step 8: ADV7535\n");
	for (i = 0; i < 3; i++) {
		ret = adv7535_start(m);
		if (!ret)
			break;
		mdss_log("  ADV7535 setup failed (%d), retrying\n", ret);
		mdelay(10);
	}
	if (ret)
		log_err("ADV7535 HDMI bridge not responding on i2c12 (%dE)\n",
			ret);

	if (mdss_verbose) {
		mdelay(200);
		hdmi_status();
	}

	return ret;
}

void mdss_display_stop(void)
{
	u32 frame;
	int i;

	if (!fb_addr)
		return;

	/* Detach DMA0 from LM0 and stop it fetching at the next vsync */
	wr(CTL0_BASE + CTL_LAYER_LM0, CTL_MIXER_BORDER_OUT);
	wr(CTL0_BASE + CTL_FETCH_PIPE_ACTIVE, 0);
	wr(CTL0_BASE + CTL_FLUSH, CTL_FLUSH_DMA0 | CTL_FLUSH_LM0 |
	   CTL_FLUSH_CTL);

	/* No timer use here: this may run inside ExitBootServices() */
	for (i = 0; i < 1000000 && rd(CTL0_BASE + CTL_FLUSH); i++)
		;
	frame = rd(INTF1_BASE + INTF_FRAME_COUNT);
	for (i = 0; i < 1000000 && rd(INTF1_BASE + INTF_FRAME_COUNT) == frame;
	     i++)
		;
	wr(INTF1_BASE + INTF_TIMING_ENGINE_EN, 0);

	/*
	 * Remove the stream mapping. The context bank keeps its S1-bypass
	 * type: writing back its original S2-translate CBAR takes a
	 * synchronous external abort (stage 2 belongs to the hypervisor), and
	 * an unused S1 bank is what the OS SMMU driver expects anyway.
	 */
	if (smmu_saved.smr >= 0) {
		wr(SMMU_BASE + SMMU_SMR(smmu_saved.smr), 0);
		wr(SMMU_BASE + SMMU_S2CR(smmu_saved.smr), smmu_saved.s2cr);
		wr(smmu_cb_base + SMMU_CB_SCTLR, smmu_saved.sctlr);
		mb();
		smmu_saved.smr = -1;
		smmu_cb_base = 0;
	}
	fb_addr = 0;
}

#if CONFIG_IS_ENABLED(CMD_QCOM_MDSS_PROBE)
static int do_mdssprobe(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	const struct mdss_mode *m = &mdss_mode_720p;
	ulong fb;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	mdss_verbose = true;
	if (!strcmp(argv[1], "info")) {
		ret = do_info();
	} else if (!strcmp(argv[1], "power")) {
		ret = do_power(argc > 2 && !strcmp(argv[2], "dsi"));
	} else if (!strcmp(argv[1], "bw")) {
		ret = do_bw();
	} else if (!strcmp(argv[1], "hdmi") && argc > 2 &&
		   !strcmp(argv[2], "status")) {
		hdmi_status();
		ret = 0;
	} else if (!strcmp(argv[1], "hdmi")) {
		ret = mdss_display_start(m, 0, argc > 2 &&
					 !strcmp(argv[2], "dsitpg"));
	} else if (!strcmp(argv[1], "fb")) {
		ret = mdss_fb_alloc(m, &fb);
		if (!ret) {
			fb_draw_test(m, fb);
			ret = mdss_display_start(m, fb, false);
		}
		mdss_log("Expect 8 colour bars (white, yellow, cyan, green, magenta, red, blue, black),\n"
			 "a grey ramp at the bottom and a white frame\n");
	} else if (!strcmp(argv[1], "stop")) {
		mdss_display_stop();
		ret = 0;
	} else if (!strcmp(argv[1], "i2c") && argc > 2 &&
		   !strcmp(argv[2], "geni")) {
		ret = do_i2c();
	} else if (!strcmp(argv[1], "i2c")) {
		ret = do_i2c_bb(argc > 2 && !strcmp(argv[2], "scan"));
	} else if (!strcmp(argv[1], "smmu") && argc > 2 &&
		   !strcmp(argv[2], "map")) {
		ret = do_smmu_map();
	} else if (!strcmp(argv[1], "smmu")) {
		ret = do_smmu();
	} else {
		mdss_verbose = false;
		return CMD_RET_USAGE;
	}
	mdss_verbose = false;

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

U_BOOT_CMD(mdssprobe, 3, 0, do_mdssprobe,
	   "probe QCS8300 display hardware",
	   "info        - read-only clock, RPMh and cmd-db state\n"
	   "mdssprobe power [dsi] - vote MMCX, power MDSS, read HW versions\n"
	   "mdssprobe bw          - vote MMNOC bandwidth, enable DISP_HF_AXI\n"
	   "mdssprobe hdmi [dsitpg] - 720p60 over DSI0 + ADV7535, no framebuffer\n"
	   "mdssprobe hdmi status - DSI, INTF and ADV7535 status\n"
	   "mdssprobe fb          - 720p60 colour bars from a framebuffer\n"
	   "mdssprobe stop        - stop scanout, as done at ExitBootServices\n"
	   "mdssprobe i2c [scan]  - bit-banged i2c12: ADV7535 IDs or bus scan\n"
	   "mdssprobe i2c geni    - ADV7535 IDs via the QUP1 SE4 engine\n"
	   "mdssprobe smmu [map]  - show / add the SMMU mapping of the MDSS SID");
#endif
