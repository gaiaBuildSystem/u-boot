// SPDX-License-Identifier: GPL-2.0+
/*
 * Environment driver for EFI media devices
 *
 * This driver allows saving/loading the U-Boot environment to/from
 * EFI media devices using the EFI Simple File System Protocol.
 */

#include <errno.h>
#include <env.h>
#include <env_internal.h>
#include <efi_api.h>
#include <efi.h>
#include <malloc.h>
#include <memalign.h>
#include <linux/errno.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_EFI_APP

/* EFI Simple File System Protocol GUID */
static const efi_guid_t efi_simple_file_system_protocol_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

#ifndef CONFIG_ENV_EFI_MEDIA_FILE
#define CONFIG_ENV_EFI_MEDIA_FILE	"uboot.env"
#endif

#ifndef CONFIG_ENV_EFI_MEDIA_DEVICE
#define CONFIG_ENV_EFI_MEDIA_DEVICE	0
#endif

__weak int env_efi_media_get_device(void)
{
	return CONFIG_ENV_EFI_MEDIA_DEVICE;
}

__weak const char *env_efi_media_get_filename(void)
{
	return CONFIG_ENV_EFI_MEDIA_FILE;
}

/**
 * efi_media_find_simple_file_system() - find EFI simple file system protocol
 *
 * @dev_idx:	device index (0, 1, 2, ...)
 * @file_system: pointer to receive the file system protocol
 * Return:	status code
 */
static efi_status_t efi_media_find_simple_file_system(int dev_idx,
	struct efi_simple_file_system_protocol **file_system)
{
	efi_status_t ret;
	efi_uintn_t num_handles;
	efi_handle_t *handles = NULL;
	struct efi_priv *priv = efi_get_priv();

	if (!priv || !priv->boot)
		return EFI_NOT_READY;

	ret = priv->boot->locate_handle_buffer(BY_PROTOCOL,
			&efi_simple_file_system_protocol_guid,
			NULL, &num_handles, &handles);
	if (ret != EFI_SUCCESS)
		return ret;

	if (dev_idx >= (int)num_handles) {
		ret = EFI_NOT_FOUND;
		goto out;
	}

	ret = priv->boot->open_protocol(handles[dev_idx],
			&efi_simple_file_system_protocol_guid,
			(void **)file_system, priv->parent_image, NULL,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL);

out:
	if (handles)
		priv->boot->free_pool(handles);
	return ret;
}

/**
 * efi_media_convert_filename() - convert filename to UTF-16
 *
 * @filename:	ASCII filename
 * Return:	UTF-16 filename (must be freed by caller)
 */
static u16 *efi_media_convert_filename(const char *filename)
{
	size_t len = strlen(filename);
	u16 *filename16;
	int i;

	filename16 = malloc((len + 1) * sizeof(u16));
	if (!filename16)
		return NULL;

	for (i = 0; i <= (int)len; i++)
		filename16[i] = filename[i];

	return filename16;
}

static int env_efi_media_save(void)
{
	env_t __aligned(ARCH_DMA_MINALIGN) env_new;
	struct efi_simple_file_system_protocol *file_system;
	struct efi_file_handle *root, *file;
	u16 *filename16;
	efi_status_t ret;
	efi_uintn_t bytes_written;
	int err;
	int dev_idx = env_efi_media_get_device();
	const char *filename = env_efi_media_get_filename();

	/* Export environment to buffer */
	err = env_export(&env_new);
	if (err)
		return err;

	/* Find EFI file system */
	ret = efi_media_find_simple_file_system(dev_idx, &file_system);
	if (ret != EFI_SUCCESS) {
		printf("EFI media device %d not found\n", dev_idx);
		return 1;
	}

	/* Open root directory */
	ret = file_system->open_volume(file_system, &root);
	if (ret != EFI_SUCCESS) {
		printf("Failed to open EFI volume\n");
		return 1;
	}

	/* Convert filename to UTF-16 */
	filename16 = efi_media_convert_filename(filename);
	if (!filename16) {
		printf("Failed to convert filename\n");
		ret = EFI_OUT_OF_RESOURCES;
		goto close_root;
	}

	/* Create/open file for writing */
	ret = root->open(root, &file, filename16,
			 EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE |
			 EFI_FILE_MODE_CREATE, 0);
	if (ret != EFI_SUCCESS) {
		printf("Failed to open file %s for writing\n", filename);
		goto free_filename;
	}

	/* Write environment to file */
	bytes_written = sizeof(env_t);
	ret = file->write(file, &bytes_written, &env_new);
	if (ret != EFI_SUCCESS || bytes_written != sizeof(env_t)) {
		printf("Failed to write environment to file\n");
		err = 1;
		goto close_file;
	}

	/* Flush file */
	ret = file->flush(file);
	if (ret != EFI_SUCCESS) {
		printf("Failed to flush file\n");
		err = 1;
		goto close_file;
	}

	printf("Environment saved to EFI media\n");
	err = 0;

close_file:
	file->close(file);
free_filename:
	free(filename16);
close_root:
	root->close(root);

	return err;
}

static int env_efi_media_load(void)
{
	ALLOC_CACHE_ALIGN_BUFFER(char, buf, CONFIG_ENV_SIZE);
	struct efi_simple_file_system_protocol *file_system;
	struct efi_file_handle *root, *file;
	u16 *filename16;
	efi_status_t ret;
	efi_uintn_t bytes_read;
	int err = 0;
	int dev_idx = env_efi_media_get_device();
	const char *filename = env_efi_media_get_filename();

	/* Find EFI file system */
	ret = efi_media_find_simple_file_system(dev_idx, &file_system);
	if (ret != EFI_SUCCESS) {
		printf("EFI media device %d not found\n", dev_idx);
		goto err_env_relocate;
	}

	/* Open root directory */
	ret = file_system->open_volume(file_system, &root);
	if (ret != EFI_SUCCESS) {
		printf("Failed to open EFI volume\n");
		goto err_env_relocate;
	}

	/* Convert filename to UTF-16 */
	filename16 = efi_media_convert_filename(filename);
	if (!filename16) {
		printf("Failed to convert filename\n");
		goto close_root;
	}

	/* Open file for reading */
	ret = root->open(root, &file, filename16, EFI_FILE_MODE_READ, 0);
	if (ret != EFI_SUCCESS) {
		debug("Environment file %s not found (first boot?)\n", filename);
		/* File not found - use default environment but return success */
		err = -ENOENT;
		goto free_filename;
	}

	/* Read environment from file */
	bytes_read = CONFIG_ENV_SIZE;
	ret = file->read(file, &bytes_read, buf);
	if (ret != EFI_SUCCESS) {
		printf("Failed to read environment from file\n");
		goto close_file;
	}

	/* Import environment */
	err = env_import(buf, 1, H_EXTERNAL);
	if (err) {
		printf("Failed to import environment\n");
		goto close_file;
	}

	gd->env_valid = ENV_VALID;

	file->close(file);
	free(filename16);
	root->close(root);

	return 0;

close_file:
	file->close(file);
free_filename:
	free(filename16);
close_root:
	root->close(root);
err_env_relocate:
	/* Return -ENOENT if environment file not found, which is normal on first boot */
	if (err == -ENOENT) {
		/* Set default environment and mark as invalid */
		env_set_default(NULL, 0);
		gd->env_valid = ENV_INVALID;
		return -ENOENT;
	}

	/* For other errors, set default and return error */
	env_set_default(NULL, 0);
	return -EIO;
}

static int env_efi_media_erase(void)
{
	struct efi_simple_file_system_protocol *file_system;
	struct efi_file_handle *root, *file;
	u16 *filename16;
	efi_status_t ret;
	int err = 0;
	int dev_idx = env_efi_media_get_device();
	const char *filename = env_efi_media_get_filename();

	/* Find EFI file system */
	ret = efi_media_find_simple_file_system(dev_idx, &file_system);
	if (ret != EFI_SUCCESS) {
		printf("EFI media device %d not found\n", dev_idx);
		return 1;
	}

	/* Open root directory */
	ret = file_system->open_volume(file_system, &root);
	if (ret != EFI_SUCCESS) {
		printf("Failed to open EFI volume\n");
		return 1;
	}

	/* Convert filename to UTF-16 */
	filename16 = efi_media_convert_filename(filename);
	if (!filename16) {
		printf("Failed to convert filename\n");
		err = 1;
		goto close_root;
	}

	/* Open file for deletion */
	ret = root->open(root, &file, filename16,
			 EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
	if (ret == EFI_SUCCESS) {
		/* Delete the file */
		ret = file->delete(file);
		if (ret != EFI_SUCCESS) {
			printf("Failed to delete environment file\n");
			err = 1;
			file->close(file);
		} else {
			printf("Environment file deleted\n");
		}
	} else if (ret == EFI_NOT_FOUND) {
		printf("Environment file not found (already erased)\n");
	} else {
		printf("Failed to open environment file for deletion\n");
		err = 1;
	}

	free(filename16);
close_root:
	root->close(root);

	if (!err)
		gd->env_valid = ENV_INVALID;

	return err;
}

static int env_efi_media_init(void)
{
	/* Mark environment as invalid initially */
	gd->env_valid = ENV_INVALID;
	return 0;
}

/**
 * env_get_location() - Return the environment location for EFI applications
 *
 * For EFI applications, we always use EFI media as the environment location.
 */
enum env_location env_get_location(enum env_operation op, int prio)
{
	/* For EFI apps, only return our EFI media environment */
	if (prio == 0)
		return ENVL_EFI_MEDIA;

	return ENVL_UNKNOWN;
}

U_BOOT_ENV_LOCATION(efi_media) = {
	.location	= ENVL_EFI_MEDIA,
	ENV_NAME("EFI_MEDIA")
	.load		= env_efi_media_load,
	.save		= ENV_SAVE_PTR(env_efi_media_save),
	.erase		= ENV_ERASE_PTR(env_efi_media_erase),
	.init		= env_efi_media_init,
};

#endif /* CONFIG_EFI_APP */
