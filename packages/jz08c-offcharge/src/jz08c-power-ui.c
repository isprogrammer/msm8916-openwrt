/* SPDX-License-Identifier: GPL-2.0-or-later
 * Minimal JZ08C preinit/off-charge framebuffer UI. Draft, not hardware-tested.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <unistd.h>

#define W 128
#define H 128
#define RAW_BYTES (W * H * 2)
#define IMG_DIR "/usr/share/jz08c-offcharge/images"
#define FBDEV "/dev/fb0"
#define BLIGHT "/sys/class/backlight/jz08c-lcd-backlight/brightness"
#define CAPACITY "/sys/class/power_supply/pm8916-bms-vm/capacity"
#define USB_ONLINE "/sys/class/power_supply/pm8916-lbc-chgr/online"

static uint16_t frame[W * H];
static uint16_t *fbmem;
static size_t fbsize;
static struct fb_fix_screeninfo fix;
static struct fb_var_screeninfo var;

static uint16_t c565(unsigned r, unsigned g, unsigned b)
{
	return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void px(int x, int y, uint16_t c)
{
	if (x >= 0 && x < W && y >= 0 && y < H) frame[y * W + x] = c;
}

static void box(int x, int y, int w, int h, uint16_t c)
{
	int xx, yy;
	for (yy = y; yy < y + h; yy++)
		for (xx = x; xx < x + w; xx++) px(xx, yy, c);
}

static const uint8_t nums[10][7] = {
	{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
	{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
	{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
	{14,17,17,15,1,2,12}
};

static void glyph(int x, int y, const uint8_t rows[7], uint16_t c)
{
	int r, col;
	for (r = 0; r < 7; r++)
		for (col = 0; col < 5; col++)
			if (rows[r] & (1 << (4 - col))) box(x + col, y + r, 1, 1, c);
}

static int read_num(const char *path)
{
	char b[32]; int n; FILE *f = fopen(path, "r");
	if (!f) return -1;
	if (!fgets(b, sizeof(b), f)) { fclose(f); return -1; }
	fclose(f);
	return sscanf(b, "%d", &n) == 1 ? n : -1;
}

static int load(const char *name)
{
	char path[256]; FILE *f; size_t n;
	snprintf(path, sizeof(path), "%s/%s", IMG_DIR, name);
	f = fopen(path, "rb"); if (!f) return -1;
	n = fread(frame, 1, RAW_BYTES, f);
	if (fgetc(f) != EOF) n = 0;
	fclose(f);
	return n == RAW_BYTES ? 0 : -1;
}

static int fb_open(void)
{
	int fd = open(FBDEV, O_RDWR);
	if (fd < 0) return -1;
	if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) ||
	    ioctl(fd, FBIOGET_VSCREENINFO, &var)) { close(fd); return -1; }
	if (var.xres != W || var.yres != H || var.bits_per_pixel != 16 ||
	    fix.line_length < W * 2 || var.red.offset != 11 ||
	    var.green.offset != 5 || var.blue.offset != 0) {
		close(fd); errno = EINVAL; return -1;
	}
	fbsize = fix.smem_len;
	if (fbsize < (size_t)fix.line_length * H) { close(fd); errno = EINVAL; return -1; }
	fbmem = mmap(NULL, fbsize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fbmem == MAP_FAILED) { fbmem = NULL; close(fd); return -1; }
	return fd;
}

static void flush(void)
{
	int y;
	for (y = 0; y < H; y++)
		memcpy((char *)fbmem + y * fix.line_length, (char *)frame + y * W * 2, W * 2);
}

static void bl(int on)
{
	FILE *f = fopen(BLIGHT, "w");
	if (f) { fprintf(f, "%d\n", on ? 1 : 0); fclose(f); }
}

static void battery(int pct)
{
	uint16_t white = c565(244,249,250), cyan = c565(0,181,226);
	char s[8]; size_t i, n; int x=36, y=57, w=54, h=28, fill;
	box(x+w,y+8,5,12,white); box(x+2,y+2,w-4,2,white);
	box(x+2,y+h-4,w-4,2,white); box(x,y,2,h,white); box(x+w-2,y,2,h,white);
	if (pct < 0) pct = 0;
	if (pct > 100) pct = 100;
	fill=(w-8)*pct/100; if (fill>0) box(x+4,y+5,fill,h-10,cyan);
	snprintf(s,sizeof(s),"%d%%",pct); n=strlen(s);
	for (i=0;i<n;i++) {
		int tx=64-(int)n*4+(int)i*8;
		if (s[i]>='0' && s[i]<='9') glyph(tx,96,nums[s[i]-'0'],white);
		else { static const uint8_t p[7]={17,2,4,8,17,0,0}; glyph(tx,96,p,white); }
	}
}

static int find_key(void)
{
	DIR *d=opendir("/sys/class/input"); struct dirent *e;
	if (!d) return -1;
	while ((e=readdir(d))) {
		char p[320], name[128], node[272]; FILE *f; int fd;
		if (strncmp(e->d_name,"event",5)) continue;
		snprintf(p,sizeof(p),"/sys/class/input/%s/device/name",e->d_name);
		f=fopen(p,"r"); if (!f) continue;
		if (!fgets(name,sizeof(name),f)) { fclose(f); continue; } fclose(f);
		if (!strstr(name,"pwrkey") && !strstr(name,"power")) continue;
		snprintf(node,sizeof(node),"/dev/input/%s",e->d_name);
		fd=open(node,O_RDONLY|O_NONBLOCK); if (fd>=0) { closedir(d); return fd; }
	}
	closedir(d); return -1;
}

static int draw_once(const char *asset)
{
	int fd=fb_open(); if (fd<0) return 1;
	if (load(asset)) return 1;
	bl(1); flush(); sleep(1);
	munmap(fbmem,fbsize); close(fd); return 0;
}

static int charging(void)
{
	int fd=fb_open(), key, rc=0; struct pollfd pfd; struct input_event ev;
	if (fd<0) return 1;
	key=find_key(); bl(1);
	for (;;) {
		if (read_num(USB_ONLINE)==0) {
			if (reboot(RB_POWER_OFF)<0) { perror("poweroff"); reboot(RB_AUTOBOOT); }
			rc=2; break;
		}
		if (load("charging-background.rgb565")) { rc=1; break; }
		{ int p=read_num(CAPACITY); if (p>=0) battery(p); }
		flush();
		if (key>=0) {
			pfd.fd=key; pfd.events=POLLIN; pfd.revents=0;
			if (poll(&pfd,1,1000)>0 && (pfd.revents&POLLIN))
				while (read(key,&ev,sizeof(ev))==sizeof(ev))
					if (ev.type==EV_KEY && ev.code==KEY_POWER && ev.value==1) goto done;
		} else sleep(1);
	}
done:
	if (key>=0) close(key);
	munmap(fbmem,fbsize); close(fd); return rc;
}

int main(int argc,char **argv)
{
	if (argc!=2) return 1;
	if (!strcmp(argv[1],"--charge")) return charging();
	if (!strcmp(argv[1],"--poweroff")) return draw_once("powering-off.rgb565");
	fprintf(stderr,"usage: %s --charge|--poweroff\n",argv[0]); return 1;
}
