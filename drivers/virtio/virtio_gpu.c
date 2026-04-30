// SPDX-License-Identifier: GPL-2.0+
/*
 * virtio-gpu-pci driver for U-Boot
 *
 * Implements a basic 2D scanout display via the virtio-gpu protocol,
 * exposing a U-Boot video uclass framebuffer.  All communication with
 * the host is synchronous (poll-until-done) – suitable for the
 * single-threaded U-Boot environment.
 *
 * Protocol reference:
 *   https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html
 *   (section 5.7 – GPU Device)
 *
 * Based on Linux drivers/gpu/drm/virtio/virtgpu_vq.c
 */

#define LOG_CATEGORY UCLASS_VIDEO

#include <common.h>
#include <dm.h>
#include <log.h>
#include <video.h>
#include <virtio_types.h>
#include <virtio.h>
#include <virtio_ring.h>
#include <linux/virtio_gpu.h>
#include <asm/global_data.h>
#include <asm/cache.h>
#include <mapmem.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * virtio-gpu exposes two virtqueues:
 *   0 – controlq: 2D/3D commands and their responses
 *   1 – cursorq:  cursor position/appearance updates
 *
 * We only need controlq for basic 2D display.
 */
#define VIRTIO_GPU_CTRLQ	0
#define VIRTIO_GPU_CURSORQ	1
#define VIRTIO_GPU_NUM_QUEUES	2

/* Arbitrary non-zero resource ID chosen by the driver. */
#define VIRTIO_GPU_RESOURCE_ID	1
#define VIRTIO_GPU_SCANOUT_ID	0

struct virtio_gpu_priv {
	struct virtqueue *ctrlq;
	struct virtqueue *cursorq;
	u32 width;
	u32 height;
};

/* --------------------------------------------------------------------------
 * Low-level command helper
 * --------------------------------------------------------------------------
 *
 * Builds a scatter-gather list of:
 *   out[0]: cmd          (host reads the command)
 *   out[1]: extra        (optional additional out data, e.g. mem_entry array)
 *   in[0]:  resp         (host writes the response)
 *
 * Kicks the control virtqueue and spins until the host returns a buffer.
 */
static int virtio_gpu_cmd(struct udevice *dev,
			   void *cmd, size_t cmd_size,
			   void *extra, size_t extra_size,
			   void *resp, size_t resp_size)
{
	struct virtio_gpu_priv *priv = dev_get_priv(dev);
	struct virtio_sg cmd_sg  = { cmd,   cmd_size  };
	struct virtio_sg xtra_sg = { extra, extra_size };
	struct virtio_sg resp_sg = { resp,  resp_size  };
	struct virtio_sg *sgs[3];
	unsigned int num_out = 0, num_in = 0;
	int ret;

	sgs[num_out++] = &cmd_sg;
	if (extra && extra_size)
		sgs[num_out++] = &xtra_sg;
	sgs[num_out + num_in++] = &resp_sg;

	ret = virtqueue_add(priv->ctrlq, sgs, num_out, num_in);
	if (ret)
		return ret;

	virtqueue_kick(priv->ctrlq);

	while (!virtqueue_get_buf(priv->ctrlq, NULL))
		;

	return 0;
}

/* --------------------------------------------------------------------------
 * VIRTIO_GPU_CMD_GET_DISPLAY_INFO
 * --------------------------------------------------------------------------
 * Queries the host for the list of active scanouts.  On success the
 * dimensions of the first enabled scanout are returned via *width/*height.
 * Returns 0 on success, -errno otherwise.
 */
static int virtio_gpu_get_display_info(struct udevice *dev,
				       u32 *width, u32 *height)
{
	struct virtio_gpu_ctrl_hdr cmd = {
		.type = cpu_to_le32(VIRTIO_GPU_CMD_GET_DISPLAY_INFO),
	};
	struct virtio_gpu_resp_display_info resp;
	int ret, i;

	memset(&resp, 0, sizeof(resp));

	ret = virtio_gpu_cmd(dev, &cmd, sizeof(cmd),
			     NULL, 0, &resp, sizeof(resp));
	if (ret)
		return ret;

	if (le32_to_cpu(resp.hdr.type) != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
		log_warning("virtio-gpu: GET_DISPLAY_INFO failed (type=0x%x)\n",
			    le32_to_cpu(resp.hdr.type));
		return -EIO;
	}

	for (i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++) {
		u32 w = le32_to_cpu(resp.pmodes[i].r.width);
		u32 h = le32_to_cpu(resp.pmodes[i].r.height);

		if (le32_to_cpu(resp.pmodes[i].enabled) && w && h) {
			*width  = w;
			*height = h;
			log_debug("virtio-gpu: scanout %d: %ux%u\n", i, w, h);
			return 0;
		}
	}

	log_warning("virtio-gpu: no enabled scanout reported by host\n");
	return -ENODEV;
}

/* --------------------------------------------------------------------------
 * VIRTIO_GPU_CMD_RESOURCE_CREATE_2D
 * --------------------------------------------------------------------------
 * Allocates a 2D pixel buffer on the host side.
 */
static int virtio_gpu_resource_create_2d(struct udevice *dev,
					  u32 resource_id, u32 format,
					  u32 width, u32 height)
{
	struct virtio_gpu_resource_create_2d cmd = {
		.hdr.type   = cpu_to_le32(VIRTIO_GPU_CMD_RESOURCE_CREATE_2D),
		.resource_id = cpu_to_le32(resource_id),
		.format      = cpu_to_le32(format),
		.width       = cpu_to_le32(width),
		.height      = cpu_to_le32(height),
	};
	struct virtio_gpu_ctrl_hdr resp;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = virtio_gpu_cmd(dev, &cmd, sizeof(cmd),
			     NULL, 0, &resp, sizeof(resp));
	if (ret)
		return ret;

	if (le32_to_cpu(resp.type) != VIRTIO_GPU_RESP_OK_NODATA) {
		log_err("virtio-gpu: RESOURCE_CREATE_2D failed (type=0x%x)\n",
			le32_to_cpu(resp.type));
		return -EIO;
	}

	return 0;
}

/* --------------------------------------------------------------------------
 * VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING
 * --------------------------------------------------------------------------
 * Associates a region of guest (U-Boot) RAM with the resource.  The host
 * will read pixel data from this address when flushing.
 *
 * The spec allows a list of memory entries; we always use exactly one.
 * The command header and the entry array are sent as separate out-buffers
 * so no contiguous allocation is required (mirrors Linux virtgpu_vq.c).
 */
static int virtio_gpu_resource_attach_backing(struct udevice *dev,
					       u32 resource_id,
					       u64 addr, u32 length)
{
	struct virtio_gpu_resource_attach_backing cmd = {
		.hdr.type    = cpu_to_le32(VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING),
		.resource_id = cpu_to_le32(resource_id),
		.nr_entries  = cpu_to_le32(1),
	};
	struct virtio_gpu_mem_entry entry = {
		.addr    = cpu_to_le64(addr),
		.length  = cpu_to_le32(length),
		.padding = 0,
	};
	struct virtio_gpu_ctrl_hdr resp;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = virtio_gpu_cmd(dev, &cmd, sizeof(cmd),
			     &entry, sizeof(entry), &resp, sizeof(resp));
	if (ret)
		return ret;

	if (le32_to_cpu(resp.type) != VIRTIO_GPU_RESP_OK_NODATA) {
		log_err("virtio-gpu: RESOURCE_ATTACH_BACKING failed (type=0x%x)\n",
			le32_to_cpu(resp.type));
		return -EIO;
	}

	return 0;
}

/* --------------------------------------------------------------------------
 * VIRTIO_GPU_CMD_SET_SCANOUT
 * --------------------------------------------------------------------------
 * Binds the resource to a display output (scanout).
 */
static int virtio_gpu_set_scanout(struct udevice *dev,
				   u32 scanout_id, u32 resource_id,
				   u32 width, u32 height)
{
	struct virtio_gpu_set_scanout cmd = {
		.hdr.type   = cpu_to_le32(VIRTIO_GPU_CMD_SET_SCANOUT),
		.r.x        = 0,
		.r.y        = 0,
		.r.width    = cpu_to_le32(width),
		.r.height   = cpu_to_le32(height),
		.scanout_id  = cpu_to_le32(scanout_id),
		.resource_id = cpu_to_le32(resource_id),
	};
	struct virtio_gpu_ctrl_hdr resp;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = virtio_gpu_cmd(dev, &cmd, sizeof(cmd),
			     NULL, 0, &resp, sizeof(resp));
	if (ret)
		return ret;

	if (le32_to_cpu(resp.type) != VIRTIO_GPU_RESP_OK_NODATA) {
		log_err("virtio-gpu: SET_SCANOUT failed (type=0x%x)\n",
			le32_to_cpu(resp.type));
		return -EIO;
	}

	return 0;
}

/* --------------------------------------------------------------------------
 * VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D
 * --------------------------------------------------------------------------
 * Notifies the host to re-read a rectangular region from the backing
 * memory and update its internal copy of the resource.
 */
static int virtio_gpu_transfer_to_host_2d(struct udevice *dev,
					   u32 resource_id,
					   u32 x, u32 y, u32 w, u32 h)
{
	struct virtio_gpu_transfer_to_host_2d cmd = {
		.hdr.type   = cpu_to_le32(VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D),
		.r.x        = cpu_to_le32(x),
		.r.y        = cpu_to_le32(y),
		.r.width    = cpu_to_le32(w),
		.r.height   = cpu_to_le32(h),
		.offset      = 0,
		.resource_id = cpu_to_le32(resource_id),
		.padding     = 0,
	};
	struct virtio_gpu_ctrl_hdr resp;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = virtio_gpu_cmd(dev, &cmd, sizeof(cmd),
			     NULL, 0, &resp, sizeof(resp));
	if (ret)
		return ret;

	if (le32_to_cpu(resp.type) != VIRTIO_GPU_RESP_OK_NODATA) {
		log_err("virtio-gpu: TRANSFER_TO_HOST_2D failed (type=0x%x)\n",
			le32_to_cpu(resp.type));
		return -EIO;
	}

	return 0;
}

/* --------------------------------------------------------------------------
 * VIRTIO_GPU_CMD_RESOURCE_FLUSH
 * --------------------------------------------------------------------------
 * Requests the host to push the resource pixels to the physical display
 * (or the QEMU window).
 */
static int virtio_gpu_resource_flush(struct udevice *dev,
				      u32 resource_id,
				      u32 x, u32 y, u32 w, u32 h)
{
	struct virtio_gpu_resource_flush cmd = {
		.hdr.type   = cpu_to_le32(VIRTIO_GPU_CMD_RESOURCE_FLUSH),
		.r.x        = cpu_to_le32(x),
		.r.y        = cpu_to_le32(y),
		.r.width    = cpu_to_le32(w),
		.r.height   = cpu_to_le32(h),
		.resource_id = cpu_to_le32(resource_id),
		.padding     = 0,
	};
	struct virtio_gpu_ctrl_hdr resp;
	int ret;

	memset(&resp, 0, sizeof(resp));

	ret = virtio_gpu_cmd(dev, &cmd, sizeof(cmd),
			     NULL, 0, &resp, sizeof(resp));
	if (ret)
		return ret;

	if (le32_to_cpu(resp.type) != VIRTIO_GPU_RESP_OK_NODATA) {
		log_err("virtio-gpu: RESOURCE_FLUSH failed (type=0x%x)\n",
			le32_to_cpu(resp.type));
		return -EIO;
	}

	return 0;
}

/* --------------------------------------------------------------------------
 * video_ops::video_sync  (called by the video console after each draw)
 * --------------------------------------------------------------------------
 * Flushes CPU caches so the host sees the latest pixel data, then
 * issues transfer-to-host + resource-flush over the control virtqueue.
 */
static int virtio_gpu_video_sync(struct udevice *dev)
{
	struct virtio_gpu_priv *priv = dev_get_priv(dev);
	struct video_priv *vid_priv = dev_get_uclass_priv(dev);
	int ret;

	/*
	 * On cached architectures we must write-back the framebuffer before
	 * telling the host to read it.  QEMU's emulated MMU sees physical
	 * memory, so a coherent flush is required on real ARM/RISC-V targets.
	 */
	if (vid_priv->flush_dcache) {
		flush_dcache_range((ulong)vid_priv->fb,
				   ALIGN((ulong)vid_priv->fb +
					 vid_priv->fb_size, ARCH_DMA_MINALIGN));
	}

	ret = virtio_gpu_transfer_to_host_2d(dev, VIRTIO_GPU_RESOURCE_ID,
					     0, 0, priv->width, priv->height);
	if (ret)
		return ret;

	return virtio_gpu_resource_flush(dev, VIRTIO_GPU_RESOURCE_ID,
					 0, 0, priv->width, priv->height);
}

/* --------------------------------------------------------------------------
 * bind
 * --------------------------------------------------------------------------
 * Called when the virtio uclass binds us as a child of the transport.
 * We advertise the framebuffer size so that video_reserve() can set aside
 * the right amount of memory before relocation.
 */
static int virtio_gpu_bind(struct udevice *dev)
{
	struct video_uc_plat *uc_plat = dev_get_uclass_plat(dev);
	struct virtio_dev_priv *uc_priv = dev_get_uclass_priv(dev->parent);

	/*
	 * Use the Kconfig-specified default dimensions.  32 bpp (4 bytes/px).
	 * If video_reserve() runs before this device is bound (e.g. because
	 * PCI virtio probing happens post-relocation) the fallback in probe()
	 * will use gd->fb_base instead.
	 */
	uc_plat->size = CONFIG_VIDEO_VIRTIO_GPU_XRES *
			CONFIG_VIDEO_VIRTIO_GPU_YRES * 4;

	/* No device-specific feature bits required for basic 2D. */
	virtio_driver_features_init(uc_priv, NULL, 0, NULL, 0);

	return 0;
}

/* --------------------------------------------------------------------------
 * probe
 * --------------------------------------------------------------------------
 * Initialises the virtqueues and sets up the 2D pipeline:
 *   1. Create a host-side resource
 *   2. Attach our framebuffer as its backing memory
 *   3. Point the scanout at the resource
 *   4. Do an initial flush so the host shows a blank frame
 */
static int virtio_gpu_probe(struct udevice *dev)
{
	struct virtio_gpu_priv *priv = dev_get_priv(dev);
	struct video_uc_plat *uc_plat = dev_get_uclass_plat(dev);
	struct video_priv *vid_priv = dev_get_uclass_priv(dev);
	struct virtqueue *vqs[VIRTIO_GPU_NUM_QUEUES];
	ulong fb_addr;
	u32 fb_size;
	int ret;

	ret = virtio_find_vqs(dev, VIRTIO_GPU_NUM_QUEUES, vqs);
	if (ret) {
		log_err("virtio-gpu: failed to find virtqueues (%d)\n", ret);
		return ret;
	}
	priv->ctrlq   = vqs[VIRTIO_GPU_CTRLQ];
	priv->cursorq = vqs[VIRTIO_GPU_CURSORQ];

	/*
	 * Query the host for the display geometry.  Fall back to the
	 * Kconfig defaults if the device reports nothing useful.
	 */
	priv->width  = CONFIG_VIDEO_VIRTIO_GPU_XRES;
	priv->height = CONFIG_VIDEO_VIRTIO_GPU_YRES;
	ret = virtio_gpu_get_display_info(dev, &priv->width, &priv->height);
	if (ret)
		log_debug("virtio-gpu: using default resolution %ux%u\n",
			  priv->width, priv->height);

	fb_size = priv->width * priv->height * 4;

	/*
	 * Resolve the framebuffer physical address.
	 *
	 * video_reserve() sets uc_plat->base before relocation when the device
	 * is already bound at that point.  If the virtio transport was only
	 * discovered post-relocation (typical for PCI), uc_plat->base is 0
	 * and we fall back to gd->fb_base – the start of the region that
	 * video_reserve() pre-allocated via CONFIG_VIDEO_PCI_DEFAULT_FB_SIZE.
	 */
	if (!uc_plat->base) {
		uc_plat->base = gd->fb_base;
		uc_plat->size = fb_size;
	}
	fb_addr = uc_plat->base;

	log_debug("virtio-gpu: fb at 0x%lx, %ux%u\n",
		  fb_addr, priv->width, priv->height);

	/* 1. Allocate a 2D resource on the host. */
	ret = virtio_gpu_resource_create_2d(dev, VIRTIO_GPU_RESOURCE_ID,
					    VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM,
					    priv->width, priv->height);
	if (ret)
		return ret;

	/* 2. Tell the host where our pixel data lives. */
	ret = virtio_gpu_resource_attach_backing(dev, VIRTIO_GPU_RESOURCE_ID,
						 (u64)fb_addr, fb_size);
	if (ret)
		return ret;

	/* 3. Connect the resource to scanout 0. */
	ret = virtio_gpu_set_scanout(dev, VIRTIO_GPU_SCANOUT_ID,
				     VIRTIO_GPU_RESOURCE_ID,
				     priv->width, priv->height);
	if (ret)
		return ret;

	/* Fill in the video uclass private data consumed by the uclass. */
	vid_priv->xsize       = (ushort)priv->width;
	vid_priv->ysize       = (ushort)priv->height;
	vid_priv->bpix        = VIDEO_BPP32;
	vid_priv->format      = VIDEO_X8R8G8B8;
	/*
	 * Under QEMU the emulated RAM is directly visible to the host; no
	 * cache flush is needed.  On real virtio hardware (e.g. a PCIe GPU
	 * card exposed inside a VM) set flush_dcache = true.
	 */
	vid_priv->flush_dcache = false;

	/*
	 * 4. Push the (cleared) framebuffer to the display so the host shows
	 *    a blank frame rather than garbage from a previous boot.
	 *    vid_priv->fb is not yet mapped here (video_post_probe() does that
	 *    after we return), so we issue the protocol commands using the
	 *    physical address directly.
	 */
	ret = virtio_gpu_transfer_to_host_2d(dev, VIRTIO_GPU_RESOURCE_ID,
					     0, 0, priv->width, priv->height);
	if (ret)
		return ret;

	return virtio_gpu_resource_flush(dev, VIRTIO_GPU_RESOURCE_ID,
					 0, 0, priv->width, priv->height);
}

static const struct video_ops virtio_gpu_video_ops = {
	.video_sync = virtio_gpu_video_sync,
};

U_BOOT_DRIVER(virtio_gpu) = {
	.name		= VIRTIO_GPU_DRV_NAME,
	.id		= UCLASS_VIDEO,
	.ops		= &virtio_gpu_video_ops,
	.bind		= virtio_gpu_bind,
	.probe		= virtio_gpu_probe,
	.priv_auto	= sizeof(struct virtio_gpu_priv),
};
