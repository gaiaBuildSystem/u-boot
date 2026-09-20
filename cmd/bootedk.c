// SPDX-License-Identifier: GPL-2.0+
/*
 * bootedk - boot an EFI_STUB kernel using the firmware's Boot Services, from
 * within U-Boot running as an EFI application.  Follows the same approach used
 * by L4TLauncher in edk2-nvidia: expose initrd via a LoadFile2 protocol handle,
 * install the FDT configuration table if provided, then call LoadImage() in
 * "from buffer" mode followed by StartImage().
 */

#define LOG_CATEGORY LOGC_EFI

#include <charset.h>
#include <command.h>
#include <env.h>
#include <efi.h>
#include <efi_api.h>
#include <log.h>
#include <malloc.h>
#include <pe.h>
#include <vsprintf.h>

/*
 * Vendor GUID used by Linux (see arm64/booting.rst in the kernel tree) to find
 * a handle exposing an initial ram disk via EFI_LOAD_FILE2_PROTOCOL.  Same GUID
 * as L4TLauncher in edk2-nvidia and EFI_INITRD_MEDIA_GUID in U-Boot's own
 * lib/efi_loader/efi_load_initrd.c.
 */
#define LINUX_EFI_INITRD_MEDIA_GUID \
	EFI_GUID(0x5568e427, 0x68fc, 0x4f3d, \
		 0xac, 0x74, 0xca, 0x55, 0x52, 0x31, 0xcc, 0x68)

/* A vendor media device path node: header + GUID payload */
struct bootedk_vendor_dp {
	struct efi_device_path dp;	/* type=4 (media), subtype=3 (vendor) */
	efi_guid_t guid;			/* LINUX_EFI_INITRD_MEDIA_GUID */
};

/*
 * Device path identifying the initrd handle: a vendor media node carrying the
 * LINUX_EFI_INITRD_MEDIA_GUID followed by an end node.  Mirrors L4TLauncher's
 * RAMDISK_DEVICE_PATH in edk2-nvidia and struct efi_lo_dp_prefix in U-Boot.
 */
struct bootedk_initrd_dp {
	struct bootedk_vendor_dp vendor;
	struct efi_device_path   end;
};

static struct bootedk_initrd_dp initrd_dp = {
	.vendor.dp = { DEVICE_PATH_TYPE_MEDIA_DEVICE,
		       DEVICE_PATH_SUB_TYPE_VENDOR_PATH },
	.vendor.guid = LINUX_EFI_INITRD_MEDIA_GUID,
	.end    = { DEVICE_PATH_TYPE_END, DEVICE_PATH_SUB_TYPE_END }
};

static void *initrd_base;
static u64   initrd_size;

/**
 * load_file2_initrd() - EFI_LOAD_FILE2_PROTOCOL::LoadFile implementation for
 *                       our in-memory initial ramdisk.
 */
static efi_status_t EFIAPI
load_file2_initrd(struct efi_load_file_protocol *this,
		  struct efi_device_path *file_path, bool boot_policy,
		  efi_uintn_t *buffer_size, void *buffer)
{
	if (boot_policy || !buffer_size)
		return EFI_INVALID_PARAMETER;
	if (!initrd_base || !initrd_size)
		return EFI_NOT_FOUND;
	if (!buffer || *buffer_size < initrd_size) {
		*buffer_size = (efi_uintn_t)initrd_size;
		return EFI_BUFFER_TOO_SMALL;
	}
	memcpy(buffer, initrd_base, initrd_size);
	*buffer_size = (efi_uintn_t)initrd_size;
	return EFI_SUCCESS;
}

static const struct efi_load_file_protocol bootedk_lf2_proto = {
	.load_file = load_file2_initrd
};

/*
 * pe_image_file_size() - Compute the on-disk size of a PE-COFF image from its
 * DOS header: SizeOfHeaders plus, for each section, the span
 * [PointerToRawData, PointerToRawData + SizeOfRawData) (max extent wins).
 * EDK2's EfiLoadImageFromBuffer validates that every raw section fits within
 * the supplied source_size.
 */
static u64 pe_image_file_size(const void *base)
{
	const IMAGE_DOS_HEADER *dos = base;
	const IMAGE_NT_HEADERS64 *nt;
	const IMAGE_SECTION_HEADER *sec;
	u32 i, size;

	if (dos->e_magic != 0x5a4d)	/* 'MZ' */
		return 0;
	nt = (const IMAGE_NT_HEADERS64 *)((const u8 *)base + dos->e_lfanew);
	if (nt->Signature != 0x00004550 ||	/* "PE\0\0" */
	    nt->OptionalHeader.Magic != 0x20b)
		return 0;

	size = nt->OptionalHeader.SizeOfHeaders;
	sec  = (const IMAGE_SECTION_HEADER *)&nt->OptionalHeader + 1;
	for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
		u64 extent = sec[i].PointerToRawData + sec[i].SizeOfRawData;
		if (extent > size)
			size = (u32)extent;
	}
	return size;
}

/*
 * install_initrd_handle() - expose @initrd_base/@size to the kernel stub via
 * the LoadFile2 protocol, following L4TLauncher's mAndroidBootImgLoadFile2.
 */
static efi_status_t install_initrd_handle(efi_handle_t *handlep)
{
	struct efi_boot_services *boot = efi_get_boot();
	efi_guid_t devpath_guid   = EFI_DEVICE_PATH_PROTOCOL_GUID;
	efi_guid_t loadfile2_guid = EFI_LOAD_FILE2_PROTOCOL_GUID;

	initrd_dp.vendor.dp.length = sizeof(initrd_dp.vendor);
	initrd_dp.end.length       = sizeof(struct efi_device_path);

	return boot->install_multiple_protocol_interfaces(
		handlep, &devpath_guid, &initrd_dp,
		&loadfile2_guid, (void *)&bootedk_lf2_proto, NULL);
}

/*
 * set_kernel_cmdline() - attach a UTF-16 command line as the loaded image's
 * LoadOptions.  The Linux EFI stub on arm64 reads its boot arguments from
 * ImageInfo->LoadOptions/Size.
 */
static int set_kernel_cmdline(efi_handle_t handle, const char *cmdline)
{
	struct efi_boot_services *boot = efi_get_boot();
	efi_guid_t loaded_image_guid  = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	struct efi_loaded_image *li;
	u16 *u16_buf, *pos;
	size_t n16, len;

	if (!cmdline || !cmdline[0])
		return 0;

	n16 = utf8_utf16_strlen(cmdline);
	len = (n16 + 1) * sizeof(u16);
	u16_buf = malloc(len);
	if (!u16_buf) {
		log_err("## Failed to allocate cmdline buffer\n");
		return -ENOMEM;
	}

	pos = u16_buf;
	if (utf8_utf16_strcpy(&pos, cmdline) < 0) {
		free(u16_buf);
		return -EINVAL;
	}

	if (boot->handle_protocol(handle, &loaded_image_guid,
				  (void **)&li)) {
		log_err("## Failed to get LoadedImage protocol\n");
		free(u16_buf);
		return -EFAULT;
	}

	li->load_options      = u16_buf;
	li->load_options_size = len;
	log_debug("## Kernel cmdline: %s\n", cmdline);
	return 0;
}

/**
 * do_bootedk() - boot an EFI_STUB kernel image via the firmware's Boot Services
 * (see L4TLauncher in edk2-nvidia for reference).
 */
static int do_bootedk(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	struct efi_boot_services *boot;
	efi_handle_t kernel_handle = 0;
	const char *cmdline;
	u64 kaddr, size, fdt_addr = 0;
	int ret, rc = -EFAULT;

	if (argc < 2 || argc > 5) {
		printf("usage: %s <kernel> [initrd[:size]] [fdt]\n",
		       cmdtp->name);
		return CMD_RET_USAGE;
	}

	/*
	 * Two accepted forms for the optional arguments after the kernel:
	 *   - colon form  (like L4TLauncher): "initrd:size" as a single arg, e.g.
	 *       bootedk <kernel> ${ramdisk_addr_r}:${ramdisk_size} [fdt]
	 *   - booti form : separate args "initrd size", e.g.
	 *       bootedk <kernel> ${ramdisk_addr_r} ${ramdisk_size} [fdt]
	 * A bare hex arg that is not paired with a size (and not an initrd:size)
	 * is taken as the device-tree address.
	 */
	bool colon_mode = false;
	int i;

	for (i = 2; i < argc; i++)
		if (strchr(argv[i], ':')) {
			colon_mode = true;
			break;
		}

	if (colon_mode) {
		for (i = 2; i < argc; i++) {
			char *c = strchr(argv[i], ':');

			if (c) {
				initrd_base = (void *)(uintptr_t)hextoul(argv[i], NULL);
				initrd_size = hextoul(c + 1, NULL);
			} else {
				fdt_addr = hextoul(argv[i], NULL);
			}
		}
	} else {
		switch (argc) {
		case 2:
			break;
		case 4:
			initrd_base = (void *)(uintptr_t)hextoul(argv[2], NULL);
			initrd_size = hextoul(argv[3], NULL);
			break;
		case 5:
			initrd_base = (void *)(uintptr_t)hextoul(argv[2], NULL);
			initrd_size = hextoul(argv[3], NULL);
			fdt_addr    = hextoul(argv[4], NULL);
			break;
		default:
			printf("usage: %s <kernel> [initrd[:size]] [fdt]\n",
			       cmdtp->name);
			return CMD_RET_USAGE;
		}
	}

	boot = efi_get_boot();
	if (!boot)
		return log_msg_ret("## not running under EFI firmware", -ENOSYS);

	cmdline = env_get("bootargs");

	kaddr = hextoul(argv[1], NULL);
	size  = pe_image_file_size((const void *)(uintptr_t)kaddr);
	log_info("## Kernel at %lx (size=%lu)\n",
		 (ulong)kaddr, (ulong)size);
	if (!size)
		return log_msg_ret("## kernel is not a valid PE-COFF image",
				   -EINVAL);

	if (fdt_addr) {
		ret = boot->install_configuration_table(&efi_guid_fdt,
							(void *)(uintptr_t)fdt_addr);
		log_info("## Installed FDT config table at %lx\n",
			 (ulong)fdt_addr);
		if (ret != EFI_SUCCESS) {
			log_err("## Failed to install FDT: r=%d\n", ret);
			return -EFAULT;
		}
	}

	if (initrd_base && initrd_size) {
		efi_handle_t initrd_handle = 0;

		log_info("## Initrd at %lx (%lu bytes)\n",
			 (ulong)initrd_base, (ulong)initrd_size);
		ret = install_initrd_handle(&initrd_handle);
		if (ret != EFI_SUCCESS) {
			log_err("## Failed to install initrd handle: r=%d\n", ret);
			return -EFAULT;
		}
	}

	/*
	 * Load the kernel image into a new object.  This installs the Loaded
	 * Image Protocol on @kernel_handle so that set_kernel_cmdline() can
	 * attach boot arguments via the Loaded Image Protocol.
	 */
	log_info("## Loading kernel image\n");
	ret = boot->load_image(true, efi_get_parent_image(), NULL,
			       (void *)(uintptr_t)kaddr, size, &kernel_handle);
	if (ret != EFI_SUCCESS) {
		log_err("## Failed to load kernel: r=%d\n", ret);
		return -EFAULT;
	}

	rc = set_kernel_cmdline(kernel_handle, cmdline);
	if (rc < 0)
		return rc;

	log_info("## Starting image\n");
	ret = boot->start_image(kernel_handle, NULL, NULL);
	if (ret != EFI_SUCCESS) {
		log_err("## start_image failed: r=%d\n", ret);
		rc = -EFAULT;
	} else {
		rc = 0;
	}

	return rc;
}

U_BOOT_CMD(bootedk, 8, 1, do_bootedk,
	   "boot an EFI_STUB kernel via firmware Boot Services",
	   "<kernel> [initrd[:size]] [fdt]\n"
	   " - initrd may be 'addr size' (two args) or 'addr:size'\n"
	   " - cmdline is read from env 'bootargs'\n");
