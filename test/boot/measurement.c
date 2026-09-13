// SPDX-License-Identifier: GPL-2.0+
/*
 * Test for measured boot functions
 *
 * Copyright 2023 IBM Corp.
 * Written by Eddie James <eajames@linux.ibm.com>
 */

#include <bootm.h>
#include <env.h>
#include <malloc.h>
#include <mapmem.h>
#include <linux/libfdt.h>
#include <tpm_tcg2.h>
#include <test/test.h>
#include <test/ut.h>

#define MEASUREMENT_TEST(_name, _flags)	\
	UNIT_TEST(_name, _flags, measurement)

static int measure(struct unit_test_state *uts)
{
	struct bootm_headers images;
	const size_t size = 1024;
	u8 *kernel;
	u8 *initrd;
	size_t i;

	kernel = malloc(size);
	initrd = malloc(size);

	images.os.image_start = map_to_sysmem(kernel);
	images.os.image_len = size;

	images.rd_start = map_to_sysmem(initrd);
	images.rd_end = images.rd_start + size;

	images.ft_addr = malloc(size);
	images.ft_len = size;

	env_set("bootargs", "measurement testing");

	for (i = 0; i < size; ++i) {
		kernel[i] = 0xf0 | (i & 0xf);
		initrd[i] = (i & 0xf0) | 0xf;
		images.ft_addr[i] = i & 0xff;
	}

	ut_assertok(bootm_measure(&images));

	free(images.ft_addr);
	free(initrd);
	free(kernel);

	return 0;
}
MEASUREMENT_TEST(measure, 0);

/* Check that the event log is passed on in the devicetree */
static int measure_fdt_log(struct unit_test_state *uts)
{
	const ulong addr = 0x12345000, size = 0x800;
	u64 raddr, rsize;
	const fdt64_t *base;
	const fdt32_t *sz;
	char fdt[1024];
	int node, len;

	/* A node with the same path as sandbox's TPM */
	ut_assertok(fdt_create_empty_tree(fdt, sizeof(fdt)));
	node = fdt_add_subnode(fdt, 0, "tpm2");
	ut_assert(node >= 0);
	ut_assertok(tcg2_fdt_set_log(fdt, addr, size));

	base = fdt_getprop(fdt, node, "linux,sml-base", &len);
	ut_assertnonnull(base);
	ut_asserteq(8, len);
	ut_asserteq_64(addr, fdt64_to_cpu(*base));
	sz = fdt_getprop(fdt, node, "linux,sml-size", &len);
	ut_assertnonnull(sz);
	ut_asserteq(4, len);
	ut_asserteq(size, fdt32_to_cpu(*sz));

	ut_asserteq(1, fdt_num_mem_rsv(fdt));
	ut_assertok(fdt_get_mem_rsv(fdt, 0, &raddr, &rsize));
	ut_asserteq_64(addr, raddr);
	ut_asserteq_64(size, rsize);

	/* A node with a different path, found by its compatible string */
	ut_assertok(fdt_create_empty_tree(fdt, sizeof(fdt)));
	node = fdt_add_subnode(fdt, 0, "security");
	ut_assert(node >= 0);
	ut_assertok(fdt_setprop_string(fdt, node, "compatible",
				       "sandbox,tpm2"));
	ut_assertok(tcg2_fdt_set_log(fdt, addr, size));
	ut_assertnonnull(fdt_getprop(fdt, node, "linux,sml-base", NULL));

	/* No TPM node at all */
	ut_assertok(fdt_create_empty_tree(fdt, sizeof(fdt)));
	ut_asserteq(-ENOENT, tcg2_fdt_set_log(fdt, addr, size));

	return 0;
}
MEASUREMENT_TEST(measure_fdt_log, 0);
