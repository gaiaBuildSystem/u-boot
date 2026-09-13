// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <image.h>
#include <lmb.h>
#include <mapmem.h>
#include <asm/global_data.h>
#include <linux/bitops.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

#define LINUX_ARM64_IMAGE_MAGIC 0x644d5241

/* See Documentation/arm64/booting.txt in the Linux kernel */
struct Image_header {
	uint32_t	code0;		/* Executable code */
	uint32_t	code1;		/* Executable code */
	uint64_t	text_offset;	/* Image load offset, LE */
	uint64_t	image_size;	/* Effective Image size, LE */
	uint64_t	flags;		/* Kernel flags, LE */
	uint64_t	res2;		/* reserved */
	uint64_t	res3;		/* reserved */
	uint64_t	res4;		/* reserved */
	uint32_t	magic;		/* Magic number */
	uint32_t	res5;
};

bool booti_is_valid(const void *img)
{
	const struct Image_header *ih = img;

	return ih->magic == le32_to_cpu(LINUX_ARM64_IMAGE_MAGIC);
}

/**
 * booti_parse() - Read the placement fields from an Image header
 *
 * @ih: Image header
 * @text_offsetp: Returns the offset of the Image from its 2MB-aligned base
 * @image_sizep: Returns the size of the Image in memory, including .bss
 * @flagsp: Returns the kernel flags
 */
static void booti_parse(const struct Image_header *ih, u64 *text_offsetp,
			u64 *image_sizep, u64 *flagsp)
{
	/*
	 * Prior to Linux commit a2c1d73b94ed, the text_offset field
	 * is of unknown endianness.  In these cases, the image_size
	 * field is zero, and we can assume a fixed value of 0x80000.
	 */
	if (ih->image_size == 0) {
		puts("Image lacks image_size field, assuming 16MiB\n");
		*image_sizep = 16 << 20;
		*text_offsetp = 0x80000;
	} else {
		*image_sizep = le64_to_cpu(ih->image_size);
		*text_offsetp = le64_to_cpu(ih->text_offset);
	}
	*flagsp = le64_to_cpu(ih->flags);
}

/**
 * booti_place() - Decide where an Image should go
 *
 * @cur: Current address of the Image, or 0 if it is not in memory yet
 * @text_offset: Offset of the Image from its 2MB-aligned base
 * @image_size: Size of the Image in memory
 * @flags: Kernel flags from the header
 * @force_reloc: Place the Image at the start of RAM regardless
 * @addrp: Returns the address for the Image, i.e. its base plus text_offset
 * Return: 0 if OK, -ENOSPC if there was not enough lmb space
 */
static int booti_place(ulong cur, u64 text_offset, u64 image_size, u64 flags,
		       bool force_reloc, ulong *addrp)
{
	bool placed = false;
	u64 dst;

	/*
	 * If bit 3 of the flags field is set, the 2MB aligned base of the
	 * kernel image can be anywhere in physical memory, so respect
	 * images->ep.  Otherwise, relocate the image to the base of RAM
	 * since memory below it is not accessible via the linear mapping.
	 */
	if (!force_reloc && (flags & BIT(3))) {
		if (IS_ENABLED(CONFIG_LMB)) {
			/* Leave the Image where it is, if that will do */
			if (cur >= text_offset &&
			    IS_ALIGNED(cur - text_offset, SZ_2M) &&
			    !lmb_alloc_addr(cur - text_offset, image_size,
					    LMB_NONE)) {
				dst = cur - text_offset;
				placed = true;
			}
			if (!placed) {
				dst = lmb_alloc(image_size, SZ_2M);
				if (!dst)
					return -ENOSPC;
			}
		} else {
			dst = cur - text_offset;
		}
	} else {
		dst = gd->dram[0].start;
	}

	*addrp = ALIGN(dst, SZ_2M) + text_offset;

	return 0;
}

int booti_setup(ulong image, ulong *relocated_addr, ulong *size,
		bool force_reloc)
{
	u64 image_size, text_offset, flags;
	struct Image_header *ih;

	*relocated_addr = image;

	ih = (struct Image_header *)map_sysmem(image, 0);

	if (!booti_is_valid(ih)) {
		puts("Bad Linux ARM64 Image magic!\n");
		return -EPERM;
	}
	booti_parse(ih, &text_offset, &image_size, &flags);
	unmap_sysmem(ih);
	*size = image_size;

	return booti_place(image, text_offset, image_size, flags, force_reloc,
			   relocated_addr);
}

/**
 * alloc_size() - Work out how much space to reserve for an Image
 *
 * An Image's .bss follows its file contents, and its size is only known once
 * the header can be read, after decompression. Allow an eighth extra for it,
 * so that the Image can normally stay where it is once its true size is known
 *
 * @size: Size of the Image file, i.e. the decompressed size
 * Return: Size to reserve
 */
static ulong alloc_size(ulong size)
{
	return size + ALIGN(size / 8, SZ_2M);
}

int booti_alloc(ulong size, ulong *addrp)
{
	phys_addr_t addr;

	if (!IS_ENABLED(CONFIG_LMB))
		return -ENOSYS;
	addr = lmb_alloc(alloc_size(size), SZ_2M);
	if (!addr)
		return -ENOSPC;
	*addrp = addr;

	return 0;
}

int booti_check(ulong image, ulong size, ulong *relocated_addr, ulong *sizep)
{
	/*
	 * Release the space so that booti_setup() can reserve exactly what the
	 * header says is needed, or move the Image if that is not possible
	 */
	lmb_free(image, alloc_size(size));

	return booti_setup(image, relocated_addr, sizep, false);
}
