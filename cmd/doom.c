// SPDX-License-Identifier: GPL-2.0+
/*
 * 'doom' command: runs the Doom engine
 *
 * Sandbox reads the WAD from the host filesystem, so run U-Boot from a
 * directory holding doom1.wad or pass an explicit path. Any other board
 * has no filesystem to hand, so the WAD is loaded into memory first and
 * its address and size given here.
 *
 *	doom [wad-file] [frame-dir]	(sandbox)
 *	doom <addr> <size>		(anywhere)
 */

#include <command.h>
#include <mapmem.h>
#include <setjmp.h>
#include <vsprintf.h>

#include <dg_store.h>

/* doomgeneric entry points (declared here to avoid pulling in the engine's
 * replacement libc headers). */
void doomgeneric_Create(int argc, char **argv);
void doomgeneric_Tick(void);
void DG_SetFrameDir(const char *dir);

/* Shared with the libc shim: exit()/I_Quit() longjmp back here. */
jmp_buf doom_exit_env;
int doom_exit_active;

static int do_doom(struct cmd_tbl *cmdtp, int flag, int argc,
		   char *const argv[])
{
	char *dargv[8];
	int dargc = 0;
	const char *wad = "doom1.wad";
	int ret;

	dg_store_reset();
	if (IS_ENABLED(CONFIG_SANDBOX)) {
		if (argc > 1)
			wad = argv[1];
		if (argc > 2)
			DG_SetFrameDir(argv[2]);
	} else {
		ulong addr, size;

		if (argc < 3) {
			printf("Load a WAD into memory first, then give its address and size\n");
			return CMD_RET_USAGE;
		}
		addr = hextoul(argv[1], NULL);
		size = hextoul(argv[2], NULL);
		if (dg_store_add_mem(wad, map_sysmem(addr, size), size)) {
			printf("doom: cannot register the WAD\n");
			return CMD_RET_FAILURE;
		}
		printf("Using the WAD at %lx (%lx bytes)\n", addr, size);
	}

	dargv[dargc++] = "doom";
	dargv[dargc++] = "-iwad";
	dargv[dargc++] = (char *)wad;

	printf("Starting DOOM with WAD '%s'. Press ESC for the menu.\n", wad);

	doom_exit_active = 1;
	ret = setjmp(doom_exit_env);
	if (ret == 0) {
		doomgeneric_Create(dargc, dargv);
		for (;;)
			doomgeneric_Tick();
	}
	doom_exit_active = 0;
	dg_store_reset();

	printf("\nDOOM exited (%d). Back to U-Boot.\n", ret);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	doom, 3, 0, do_doom,
	"run the DOOM game",
	"[wad-file] [frame-dir]  (sandbox)\n"
	"    - Run DOOM, loading the given WAD (default: doom1.wad).\n"
	"      Arrows move, Ctrl fires, Space opens doors, ESC for menu.\n"
	"      If frame-dir is given, periodic frames are written there as\n"
	"      PPM images (useful on a headless host).\n"
	"doom <addr> <size>  (other boards)\n"
	"    - Run DOOM with a WAD already loaded into memory.\n"
	"      WASD or the arrows move, F fires, Z and X strafe,\n"
	"      Space opens doors, ESC for menu."
);
