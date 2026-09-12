// SPDX-License-Identifier: GPL-2.0+
/*
 * doomgeneric platform layer for U-Boot
 *
 * Implements the six DG_*() hooks the Doom engine needs: a framebuffer
 * to present, millisecond timing, a sleep and keyboard input. Video goes
 * through the DM video uclass.
 *
 * Sandbox reads the keyboard through the SDL key scanner, which gives
 * real press and release events. Any other board has only the console,
 * which reports a key going down and never coming up, so a press there
 * is released again shortly afterwards.
 */

#include <dm.h>
#include <video.h>
#include <time.h>
#include <stdio.h>
#include <vsprintf.h>
#include <linux/delay.h>
#ifdef CONFIG_SANDBOX
#include <os.h>
#include <asm/sdl.h>
#endif

#include "engine/doomgeneric.h"
#include "engine/doomkeys.h"

/* ---- video ----------------------------------------------------------- */

static struct udevice *doom_vid;
static int blit_xoff, blit_yoff;

static void DG_PumpKeys(void);

/* ---- optional frame recording (sandbox) ------------------------------ */

#ifdef CONFIG_SANDBOX

static const char *dump_dir;
static unsigned int dump_count;

void DG_SetFrameDir(const char *dir)
{
	dump_dir = dir;
}

/*
 * Write the current frame to <dir>/doom-NNNN.ppm. One frame in every 30
 * is saved (~once a second), up to a small cap, so the engine output can
 * be inspected on a headless host with no SDL display.
 */
static void dump_frame(void)
{
	static unsigned char row[DOOMGENERIC_RESX * 3];
	static int frame;
	char path[256];
	char hdr[64];
	int fd, x, y, n;

	frame++;
	if (!dump_dir || (frame % 30) || dump_count >= 30)
		return;

	snprintf(path, sizeof(path), "%s/doom-%04d.ppm", dump_dir, frame);
	fd = os_open(path, OS_O_WRONLY | OS_O_CREAT | OS_O_TRUNC);
	if (fd < 0)
		return;

	n = snprintf(hdr, sizeof(hdr), "P6\n%d %d\n255\n",
		     DOOMGENERIC_RESX, DOOMGENERIC_RESY);
	os_write(fd, hdr, n);
	for (y = 0; y < DOOMGENERIC_RESY; y++) {
		u32 *src = DG_ScreenBuffer + (long)y * DOOMGENERIC_RESX;

		for (x = 0; x < DOOMGENERIC_RESX; x++) {
			u32 p = src[x];

			row[x * 3 + 0] = (p >> 16) & 0xff;
			row[x * 3 + 1] = (p >> 8) & 0xff;
			row[x * 3 + 2] = p & 0xff;
		}
		os_write(fd, row, DOOMGENERIC_RESX * 3);
	}
	os_close(fd);
	dump_count++;
	printf("doom: wrote %s\n", path);
}

#else /* !CONFIG_SANDBOX */

void DG_SetFrameDir(const char *dir)
{
	printf("doom: frame recording needs sandbox\n");
}

static void dump_frame(void)
{
}

#endif /* CONFIG_SANDBOX */

void DG_Init(void)
{
	struct video_priv *priv;
	int ret;

	ret = uclass_get_device(UCLASS_VIDEO, 0, &doom_vid);
	if (ret) {
		printf("doom: no video device (err %d)\n", ret);
		doom_vid = NULL;
		return;
	}
	priv = dev_get_uclass_priv(doom_vid);

	blit_xoff = ((int)priv->xsize - DOOMGENERIC_RESX) / 2;
	blit_yoff = ((int)priv->ysize - DOOMGENERIC_RESY) / 2;
	if (blit_xoff < 0)
		blit_xoff = 0;
	if (blit_yoff < 0)
		blit_yoff = 0;
}

void DG_DrawFrame(void)
{
	struct video_priv *priv;
	int rows = DOOMGENERIC_RESY;
	int cols = DOOMGENERIC_RESX;
	int y;

	/* Record frames even on a headless host with no video device. */
	dump_frame();

	if (!doom_vid)
		return;
	priv = dev_get_uclass_priv(doom_vid);

	if (cols > priv->xsize)
		cols = priv->xsize;
	if (rows > priv->ysize)
		rows = priv->ysize;

	/*
	 * The engine has rendered DOOMGENERIC_RESX x RESY pixels as 0x00RRGGBB,
	 * which is what video_index_to_colour() produces for every 32bpp format
	 * apart from VIDEO_RGBA8888, so those go straight into the framebuffer.
	 * Note that VIDEO_X8B8G8R8 is among them: the video uclass packs it the
	 * same way as VIDEO_X8R8G8B8, whatever its name suggests. Each row is
	 * placed honouring the stride, centring the image on the panel.
	 */
	for (y = 0; y < rows; y++) {
		void *dst = priv->fb +
			    (blit_yoff + y) * priv->line_length +
			    blit_xoff * sizeof(u32);
		u32 *src = DG_ScreenBuffer + (long)y * DOOMGENERIC_RESX;

		if (priv->format == VIDEO_RGBA8888) {
			u32 *out = dst;
			int x;

			for (x = 0; x < cols; x++)
				out[x] = (src[x] << 8) | 0xff;
		} else {
			memcpy(dst, src, cols * sizeof(u32));
		}
	}

	video_sync(doom_vid, true);

	DG_PumpKeys();
}

void DG_SetWindowTitle(const char *title)
{
	(void)title;
}

/* ---- timing ---------------------------------------------------------- */

uint32_t DG_GetTicksMs(void)
{
	return (uint32_t)get_timer(0);
}

void DG_SleepMs(uint32_t ms)
{
	mdelay(ms);
}

/* ---- keyboard -------------------------------------------------------- */

/* Linux input-event scancodes (stable ABI) returned by the SDL scanner. */
enum {
	SC_ESC = 1, SC_1 = 2, SC_9 = 10, SC_0 = 11, SC_MINUS = 12,
	SC_EQUAL = 13, SC_BACKSPACE = 14, SC_TAB = 15,
	SC_Q = 16, SC_P = 25, SC_ENTER = 28, SC_LCTRL = 29,
	SC_A = 30, SC_L = 38, SC_LSHIFT = 42, SC_Z = 44, SC_M = 50,
	SC_RSHIFT = 54, SC_LALT = 56, SC_SPACE = 57,
	SC_F1 = 59, SC_F10 = 68, SC_F11 = 87, SC_F12 = 88,
	SC_RCTRL = 97, SC_RALT = 100,
	SC_UP = 103, SC_LEFT = 105, SC_RIGHT = 106, SC_DOWN = 108,
};

/* Map a contiguous letter-row scancode to its lowercase ASCII (cheats). */
static int letter_ascii(int sc)
{
	static const char row1[] = "qwertyuiop";	/* 16..25 */
	static const char row2[] = "asdfghjkl";		/* 30..38 */
	static const char row3[] = "zxcvbnm";		/* 44..50 */

	if (sc >= 16 && sc <= 25)
		return row1[sc - 16];
	if (sc >= 30 && sc <= 38)
		return row2[sc - 30];
	if (sc >= 44 && sc <= 50)
		return row3[sc - 44];
	return 0;
}

static unsigned char scancode_to_doom(int sc)
{
	switch (sc) {
	case SC_UP:	return KEY_UPARROW;
	case SC_DOWN:	return KEY_DOWNARROW;
	case SC_LEFT:	return KEY_LEFTARROW;
	case SC_RIGHT:	return KEY_RIGHTARROW;
	case SC_LCTRL:
	case SC_RCTRL:	return KEY_FIRE;
	case SC_SPACE:	return KEY_USE;
	case SC_LSHIFT:
	case SC_RSHIFT:	return KEY_RSHIFT;
	case SC_LALT:
	case SC_RALT:	return KEY_LALT;
	case SC_ESC:	return KEY_ESCAPE;
	case SC_ENTER:	return KEY_ENTER;
	case SC_TAB:	return KEY_TAB;
	case SC_BACKSPACE: return KEY_BACKSPACE;
	case SC_MINUS:	return KEY_MINUS;
	case SC_EQUAL:	return KEY_EQUALS;
	case SC_F1:	return KEY_F1;
	case SC_F1 + 1:	return KEY_F2;
	case SC_F1 + 2:	return KEY_F3;
	case SC_F1 + 3:	return KEY_F4;
	case SC_F1 + 4:	return KEY_F5;
	case SC_F1 + 5:	return KEY_F6;
	case SC_F1 + 6:	return KEY_F7;
	case SC_F1 + 7:	return KEY_F8;
	case SC_F1 + 8:	return KEY_F9;
	case SC_F10:	return KEY_F10;
	case SC_F11:	return KEY_F11;
	case SC_F12:	return KEY_F12;
	}
	if (sc >= SC_1 && sc <= SC_9)
		return '1' + (sc - SC_1);
	if (sc == SC_0)
		return '0';
	return letter_ascii(sc);
}

#define MAX_KEYS	32
#define KEYQ_SIZE	64

static int prev_keys[MAX_KEYS];
static int prev_count;

static unsigned short keyq[KEYQ_SIZE];
static unsigned int keyq_wr, keyq_rd;

static void keyq_push(int pressed, unsigned char key)
{
	if (!key)
		return;
	keyq[keyq_wr] = (pressed << 8) | key;
	keyq_wr = (keyq_wr + 1) % KEYQ_SIZE;
}

static int in_set(const int *set, int n, int sc)
{
	int i;

	for (i = 0; i < n; i++)
		if (set[i] == sc)
			return 1;
	return 0;
}

#ifdef CONFIG_SANDBOX

/* Scan the keyboard and turn level-triggered key state into events. */
static void DG_PumpKeys(void)
{
	int cur[MAX_KEYS];
	int n, i;

	n = sandbox_sdl_scan_keys(cur, MAX_KEYS);
	if (n < 0)
		n = 0;

	for (i = 0; i < n; i++)
		if (!in_set(prev_keys, prev_count, cur[i]))
			keyq_push(1, scancode_to_doom(cur[i]));

	for (i = 0; i < prev_count; i++)
		if (!in_set(cur, n, prev_keys[i]))
			keyq_push(0, scancode_to_doom(prev_keys[i]));

	for (i = 0; i < n; i++)
		prev_keys[i] = cur[i];
	prev_count = n;
}

#else /* !CONFIG_SANDBOX */

/*
 * How long a key stays down. The console says when a key is pressed but
 * never when it is let go, so each press is released again after this
 * long, and holding a key down works because the auto-repeat arrives
 * before the release does
 */
#define KEY_HOLD_MS	140

/* keys which are down, waiting to be released again */
static struct {
	unsigned char key;
	unsigned long due;
} held[MAX_KEYS];
static int held_count;

static void press(unsigned char key)
{
	int i;

	if (!key)
		return;

	/* a key which is already down just has its release put off */
	for (i = 0; i < held_count; i++) {
		if (held[i].key == key) {
			held[i].due = get_timer(0) + KEY_HOLD_MS;
			return;
		}
	}
	if (held_count == MAX_KEYS)
		return;

	keyq_push(1, key);
	held[held_count].key = key;
	held[held_count].due = get_timer(0) + KEY_HOLD_MS;
	held_count++;
}

/* map a character from the console to the key the engine knows */
static unsigned char char_to_doom(int ch)
{
	switch (ch) {
	case ' ':	return KEY_USE;
	case '\r':
	case '\n':	return KEY_ENTER;
	case '\t':	return KEY_TAB;
	case 0x7f:
	case '\b':	return KEY_BACKSPACE;
	/* no way to report a modifier, so give fire and strafe their own keys */
	case 'f':
	case 'F':	return KEY_FIRE;
	case 'z':
	case 'Z':	return KEY_STRAFE_L;
	case 'x':
	case 'X':	return KEY_STRAFE_R;
	case 'w':
	case 'W':	return KEY_UPARROW;
	case 's':
	case 'S':	return KEY_DOWNARROW;
	case 'a':
	case 'A':	return KEY_LEFTARROW;
	case 'd':
	case 'D':	return KEY_RIGHTARROW;
	}
	if (ch >= '0' && ch <= '9')
		return ch;
	if (ch > 0 && ch < 0x80)
		return ch;

	return 0;
}

/*
 * Read whatever the console has and turn it into key presses. An arrow
 * key arrives as the three characters ESC [ A, so a lone ESC is only the
 * menu key if nothing follows it
 */
static void DG_PumpKeys(void)
{
	unsigned long now = get_timer(0);
	int i;

	while (tstc()) {
		int ch = getchar();

		if (ch == 0x1b && tstc()) {
			ch = getchar();
			if (ch != '[') {
				press(KEY_ESCAPE);
				press(char_to_doom(ch));
				continue;
			}
			if (!tstc())
				continue;
			switch (getchar()) {
			case 'A':
				press(KEY_UPARROW);
				break;
			case 'B':
				press(KEY_DOWNARROW);
				break;
			case 'C':
				press(KEY_RIGHTARROW);
				break;
			case 'D':
				press(KEY_LEFTARROW);
				break;
			}
			continue;
		}
		if (ch == 0x1b)
			press(KEY_ESCAPE);
		else
			press(char_to_doom(ch));
	}

	/* let go of anything which has been down long enough */
	for (i = 0; i < held_count; i++) {
		if ((long)(now - held[i].due) < 0)
			continue;
		keyq_push(0, held[i].key);
		held[i] = held[--held_count];
		i--;
	}
}

#endif /* CONFIG_SANDBOX */

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
	if (keyq_rd == keyq_wr)
		return 0;

	*pressed = keyq[keyq_rd] >> 8;
	*doomKey = keyq[keyq_rd] & 0xff;
	keyq_rd = (keyq_rd + 1) % KEYQ_SIZE;
	return 1;
}
