// SPDX-License-Identifier: GPL-2.0+
/*
 * Tests for the emulated NVMe controller
 *
 * These run the real NVMe driver against the sandbox emulator, so they cover
 * the register and queue handling as well as the block interface.
 *
 * Copyright 2026 Simon Glass <sjg@chromium.org>
 */

#include <blk.h>
#include <dm.h>
#include <nvme.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

/* the emulator provides this much when no backing file is named */
#define NVME_TEST_BLOCKS	4096
#define NVME_TEST_BLOCK_SIZE	512

/**
 * get_blk() - Scan for namespaces and return the first block device
 *
 * @uts: Test state
 * @blkp: Returns the block device
 * Return: 0 if OK, other value on error
 */
static int get_blk(struct unit_test_state *uts, struct udevice **blkp)
{
	ut_assertok(nvme_scan_namespace());
	ut_assertok(blk_get_device(UCLASS_NVME, 0, blkp));

	return 0;
}

/* Test that the driver brings the emulated controller up */
static int dm_test_nvme_probe(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_get_device(UCLASS_NVME, 0, &dev));
	ut_asserteq_str("sandbox_nvme", dev->driver->name);

	return 0;
}
DM_TEST(dm_test_nvme_probe, UTF_SCAN_FDT);

/* Test that Identify reports the namespace the emulator offers */
static int dm_test_nvme_identify(struct unit_test_state *uts)
{
	struct blk_desc *desc;
	struct udevice *blk;

	ut_assertok(get_blk(uts, &blk));
	desc = dev_get_uclass_plat(blk);

	ut_asserteq(NVME_TEST_BLOCK_SIZE, desc->blksz);
	ut_asserteq(NVME_TEST_BLOCKS, desc->lba);
	ut_asserteq_str("0123456789ab", desc->product);

	return 0;
}
DM_TEST(dm_test_nvme_identify, UTF_SCAN_FDT);

/* Test that a block written through the driver reads back */
static int dm_test_nvme_rw(struct unit_test_state *uts)
{
	u8 out[NVME_TEST_BLOCK_SIZE], in[NVME_TEST_BLOCK_SIZE];
	struct udevice *blk;
	int i;

	ut_assertok(get_blk(uts, &blk));

	for (i = 0; i < NVME_TEST_BLOCK_SIZE; i++)
		out[i] = i;
	ut_asserteq(1, blk_write(blk, 2, 1, out));

	memset(in, '\0', sizeof(in));
	ut_asserteq(1, blk_read(blk, 2, 1, in));
	ut_asserteq_mem(out, in, sizeof(out));

	/* a different block is untouched by that write */
	ut_asserteq(1, blk_read(blk, 3, 1, in));
	for (i = 0; i < NVME_TEST_BLOCK_SIZE; i++)
		ut_asserteq(0, in[i]);

	return 0;
}
DM_TEST(dm_test_nvme_rw, UTF_SCAN_FDT);

/* Test that the driver refuses a transfer past the end of the namespace */
static int dm_test_nvme_range(struct unit_test_state *uts)
{
	u8 buf[NVME_TEST_BLOCK_SIZE];
	struct udevice *blk;

	ut_assertok(get_blk(uts, &blk));

	/* the last block is there, the one after it is not */
	ut_asserteq(1, blk_read(blk, NVME_TEST_BLOCKS - 1, 1, buf));
	ut_asserteq(0, blk_read(blk, NVME_TEST_BLOCKS, 1, buf));

	return 0;
}
DM_TEST(dm_test_nvme_range, UTF_SCAN_FDT);
