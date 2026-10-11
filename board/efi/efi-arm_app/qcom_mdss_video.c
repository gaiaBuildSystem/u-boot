// SPDX-License-Identifier: GPL-2.0+
/*
 * Video driver for the HDMI output of the Arduino Monza (QCS8300) board when
 * running as an EFI app, where UEFI provides no graphics output protocol
 *
 * The display is driven directly by qcom_mdss.c. By default scanout is
 * stopped when the OS calls ExitBootServices(), whether it is booted by
 * U-Boot or through its own EFI stub, so the OS display driver starts from
 * a quiet state. With CONFIG_VIDEO_QCOM_MDSS_EFI_KEEP_SCANOUT it keeps going
 * from reserved memory, so the splash stays up until the OS takes over.
 */

#define LOG_CATEGORY UCLASS_VIDEO

#include <cpu_func.h>
#include <dm.h>
#include <efi.h>
#include <efi_api.h>
#include <event.h>
#include <log.h>
#include <video.h>
#include <linux/string.h>
#include "qcom_mdss.h"

static void EFIAPI qcom_mdss_exit_boot_services(struct efi_event *event,
						void *context)
{
	mdss_display_stop();
}

static int qcom_mdss_video_probe(struct udevice *dev)
{
	struct video_uc_plat *plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct efi_boot_services *boot = efi_get_boot();
	const struct mdss_mode *m = &mdss_mode_720p;
	struct efi_event *event;
	efi_status_t status;
	ulong fb;
	int ret;

	ret = mdss_fb_alloc(m, &fb);
	if (ret)
		return log_msg_ret("fb", ret);

	/* Start black rather than showing whatever the memory held */
	memset((void *)fb, '\0', m->hact * m->vact * 4);
	flush_dcache_range(fb, fb + m->hact * m->vact * 4);

	ret = mdss_display_start(m, fb, false);
	if (ret) {
		log_err("HDMI display failed to start (err=%dE)\n", ret);
		return log_msg_ret("start", ret);
	}

	if (!IS_ENABLED(CONFIG_VIDEO_QCOM_MDSS_EFI_KEEP_SCANOUT)) {
		status = boot->create_event(EVT_SIGNAL_EXIT_BOOT_SERVICES,
					    TPL_NOTIFY,
					    qcom_mdss_exit_boot_services, NULL,
					    &event);
		if (status) {
			/*
			 * The framebuffer is boot-services memory the OS will
			 * reuse, so it must not be scanned out any more
			 */
			log_err("Cannot hook ExitBootServices (%lx)\n", status);
			mdss_display_stop();
			return log_msg_ret("evt", -ENXIO);
		}
	}

	plat->base = fb;
	plat->size = m->hact * m->vact * 4;
	uc_priv->xsize = m->hact;
	uc_priv->ysize = m->vact;
	uc_priv->bpix = VIDEO_BPP32;
	uc_priv->format = VIDEO_X8R8G8B8;
	uc_priv->line_length = m->hact * 4;
	video_set_flush_dcache(dev, true);
	log_debug("HDMI %ux%u framebuffer at %lx\n", m->hact, m->vact, fb);

	return 0;
}

/*
 * Start the display at boot even when the console is not on it (e.g.
 * stdout=serial), so a splash image can be shown. This cannot be done at
 * bind time: probing creates a vidconsole, which needs stdio to be set up.
 */
static int qcom_mdss_video_start(void)
{
	struct udevice *dev;

	if (uclass_first_device_err(UCLASS_VIDEO, &dev))
		log_warning("No HDMI display\n");

	return 0;
}
EVENT_SPY_SIMPLE(EVT_LAST_STAGE_INIT, qcom_mdss_video_start);

static const struct udevice_id qcom_mdss_video_ids[] = {
	{ .compatible = "arduino,monza-efi-hdmi" },
	{ }
};

U_BOOT_DRIVER(qcom_mdss_video) = {
	.name		= "qcom_mdss_video",
	.id		= UCLASS_VIDEO,
	.of_match	= qcom_mdss_video_ids,
	.probe		= qcom_mdss_video_probe,
};
