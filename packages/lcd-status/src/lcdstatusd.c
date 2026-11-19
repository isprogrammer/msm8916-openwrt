/* SPDX-License-Identifier: GPL-2.0-only */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_LINES 32
#define MAX_TEXT 256
#define MAX_LABEL 32
#define DEFAULT_CONFIG "/etc/config/lcdstatus"

struct lcd_line {
	char label[MAX_LABEL];
	char source[MAX_TEXT];
	char color[16];
	char exec_cmd[MAX_TEXT];
};
struct settings {
	int enabled, refresh_s, rotation, font_scale, backlight;
	char fg[16], bg[16];
	struct lcd_line lines[MAX_LINES];
	size_t nlines;
};

static volatile sig_atomic_t stopping;
static void on_signal(int sig) { (void)sig; stopping = 1; }

static void trim(char *s)
{
	size_t n = strlen(s);
	while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
	char *p = s;
	while (*p && isspace((unsigned char)*p)) p++;
	if (p != s) memmove(s, p, strlen(p) + 1);
}

static int token(char **cursor, char *out, size_t cap)
{
	char *p = *cursor;
	size_t n = 0;
	while (*p && isspace((unsigned char)*p)) p++;
	if (!*p || *p == '#') { *cursor = p + strlen(p); return 0; }
	char quote = 0;
	if (*p == '\'' || *p == '"') quote = *p++;
	while (*p) {
		if (quote) {
			if (*p == quote) { p++; break; }
		} else if (isspace((unsigned char)*p) || *p == '#') {
			break;
		}
		if (*p == '\\' && p[1]) p++;
		if (n + 1 < cap) out[n++] = *p;
		p++;
	}
	out[n] = '\0';
	while (*p && isspace((unsigned char)*p)) p++;
	if (*p == '#') p += strlen(p);
	*cursor = p;
	return 1;
}

static void setting_value(struct settings *s, const char *section,
                          const char *key, const char *value)
{
	if (!strcmp(section, "global")) {
		if (!strcmp(key, "enabled")) s->enabled = atoi(value) != 0;
		else if (!strcmp(key, "refresh_s")) s->refresh_s = atoi(value);
		else if (!strcmp(key, "rotation")) s->rotation = atoi(value);
		else if (!strcmp(key, "font_scale")) s->font_scale = atoi(value);
		else if (!strcmp(key, "backlight_brightness")) s->backlight = atoi(value);
		else if (!strcmp(key, "fg")) snprintf(s->fg, sizeof(s->fg), "%s", value);
		else if (!strcmp(key, "bg")) snprintf(s->bg, sizeof(s->bg), "%s", value);
	} else if (!strcmp(section, "line") && s->nlines) {
		struct lcd_line *l = &s->lines[s->nlines - 1];
		if (!strcmp(key, "label")) snprintf(l->label, sizeof(l->label), "%s", value);
		else if (!strcmp(key, "source")) snprintf(l->source, sizeof(l->source), "%s", value);
		else if (!strcmp(key, "color")) snprintf(l->color, sizeof(l->color), "%s", value);
		else if (!strcmp(key, "command")) snprintf(l->exec_cmd, sizeof(l->exec_cmd), "%s", value);
	}
}

static void defaults(struct settings *s)
{
	memset(s, 0, sizeof(*s));
	s->enabled = 1; s->refresh_s = 5; s->font_scale = 1;
	s->backlight = -1;
	strcpy(s->fg, "FFFFFF"); strcpy(s->bg, "000000");
}

static int load_settings(struct settings *s, const char *path)
{
	FILE *f;
	char line[1024], a[64], b[256], c[512];
	char section[32] = "";
	defaults(s);
	f = fopen(path, "r");
	if (!f) return errno == ENOENT ? 0 : -1;
	while (fgets(line, sizeof(line), f)) {
		char *p = line;
		if (!token(&p, a, sizeof(a))) continue;
		if (!strcmp(a, "config")) {
			if (!token(&p, b, sizeof(b))) continue;
			snprintf(section, sizeof(section), "%s", b);
			if (!strcmp(section, "line") && s->nlines < MAX_LINES) {
				struct lcd_line *l = &s->lines[s->nlines++];
				memset(l, 0, sizeof(*l)); strcpy(l->color, "FFFFFF");
			}
		} else if (!strcmp(a, "option")) {
			if (token(&p, b, sizeof(b)) && token(&p, c, sizeof(c)))
				setting_value(s, section, b, c);
		}
	}
	fclose(f);
	if (s->refresh_s < 1) s->refresh_s = 1;
	if (s->refresh_s > 3600) s->refresh_s = 3600;
	if (s->font_scale != 2) s->font_scale = 1;
	if (s->rotation != 180) s->rotation = 0;
	for (size_t i = 0; i < s->nlines; i++) {
		if (!strcmp(s->lines[i].source, "exec") && s->lines[i].exec_cmd[0]) {
			snprintf(s->lines[i].source, sizeof(s->lines[i].source),
			         "exec:%s", s->lines[i].exec_cmd);
		}
	}
	return 0;
}

struct glyph { char ch; uint8_t row[7]; };
#define G(c,a,b,d,e,f,g,h) {c,{a,b,d,e,f,g,h}}
static const struct glyph font[] = {
	G('A',14,17,17,31,17,17,17), G('B',30,17,17,30,17,17,30),
	G('C',14,17,16,16,16,17,14), G('D',30,17,17,17,17,17,30),
	G('E',31,16,16,30,16,16,31), G('F',31,16,16,30,16,16,16),
	G('G',14,17,16,23,17,17,15), G('H',17,17,17,31,17,17,17),
	G('I',14,4,4,4,4,4,14), G('J',7,2,2,2,18,18,12),
	G('K',17,18,20,24,20,18,17), G('L',16,16,16,16,16,16,31),
	G('M',17,27,21,21,17,17,17), G('N',17,25,21,19,17,17,17),
	G('O',14,17,17,17,17,17,14), G('P',30,17,17,30,16,16,16),
	G('Q',14,17,17,17,21,18,13), G('R',30,17,17,30,20,18,17),
	G('S',15,16,16,14,1,1,30), G('T',31,4,4,4,4,4,4),
	G('U',17,17,17,17,17,17,14), G('V',17,17,17,17,17,10,4),
	G('W',17,17,17,21,21,21,10), G('X',17,17,10,4,10,17,17),
	G('Y',17,17,10,4,4,4,4), G('Z',31,1,2,4,8,16,31),
	G('0',14,17,19,21,25,17,14), G('1',4,12,4,4,4,4,14),
	G('2',14,17,1,2,4,8,31), G('3',30,1,1,14,1,1,30),
	G('4',2,6,10,18,31,2,2), G('5',31,16,16,30,1,1,30),
	G('6',14,16,16,30,17,17,14), G('7',31,1,2,4,8,8,8),
	G('8',14,17,17,14,17,17,14), G('9',14,17,17,15,1,1,14),
	G(':',0,4,4,0,4,4,0), G('.',0,0,0,0,0,6,6),
	G('/',1,2,2,4,8,8,16), G('-',0,0,0,31,0,0,0),
	G('_',0,0,0,0,0,0,31), G('%',17,2,4,8,17,0,0),
	G('+',0,4,4,31,4,4,0), G('?',14,17,1,2,4,0,4),
	G(',',0,0,0,0,0,4,8), G('=',0,0,31,0,31,0,0),
	G('(',2,4,8,8,8,4,2), G(')',8,4,2,2,2,4,8),
	G('[',14,8,8,8,8,8,14), G(']',14,2,2,2,2,2,14),
	G(' ',0,0,0,0,0,0,0)
};
#undef G

static const uint8_t *glyph_rows(char ch)
{
	size_t i;
	if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
	for (i = 0; i < sizeof(font) / sizeof(font[0]); i++)
		if (font[i].ch == ch) return font[i].row;
	for (i = 0; i < sizeof(font) / sizeof(font[0]); i++)
		if (font[i].ch == '?') return font[i].row;
	return font[sizeof(font) / sizeof(font[0]) - 1].row;
}

static uint16_t color565(const char *s, uint16_t fallback)
{
	unsigned int v;
	if (*s == '#') s++;
	if (strlen(s) != 6 || sscanf(s, "%x", &v) != 1) return fallback;
	return (uint16_t)(((v >> 19) & 0x1f) << 11 |
	                  ((v >> 10) & 0x3f) << 5 | ((v >> 3) & 0x1f));
}

static void put_pixel(uint8_t *frame, int width, int height, int stride,
                      int rotation, int x, int y, uint16_t c)
{
	int tx = x, ty = y;
	if (x < 0 || y < 0 || x >= width || y >= height) return;
	if (rotation == 180) { tx = width - 1 - x; ty = height - 1 - y; }
	memcpy(frame + (size_t)ty * stride + (size_t)tx * 2, &c, sizeof(c));
}

static void fill_rect(uint8_t *frame, int w, int h, int stride, int rotation,
                      int x, int y, int rw, int rh, uint16_t c)
{
	int yy, xx;
	for (yy = 0; yy < rh; yy++)
		for (xx = 0; xx < rw; xx++)
			put_pixel(frame, w, h, stride, rotation, x + xx, y + yy, c);
}

static void draw_text(uint8_t *frame, int w, int h, int stride, int rotation,
                      int x, int y, const char *text, uint16_t fg, uint16_t bg,
                      int scale)
{
	for (; *text && x + 5 * scale < w; text++, x += 6 * scale) {
		const uint8_t *rows = glyph_rows(*text);
		int r, c, sy, sx;
		for (r = 0; r < 7; r++) for (c = 0; c < 5; c++) {
			uint16_t col = (rows[r] & (1u << (4 - c))) ? fg : bg;
			for (sy = 0; sy < scale; sy++) for (sx = 0; sx < scale; sx++)
				put_pixel(frame, w, h, stride, rotation,
				          x + c * scale + sx, y + r * scale + sy, col);
		}
	}
}

static int read_small(const char *path, char *out, size_t cap)
{
	FILE *f = fopen(path, "r");
	if (!f) return -1;
	if (!fgets(out, (int)cap, f)) { fclose(f); return -1; }
	fclose(f); trim(out); return 0;
}

static int run_capture(const char *command, char *out, size_t cap, int timeout_ms);

static int wifi_clients(void)
{
	char out[8192], ifaces[128][IFNAMSIZ], cmd[256];
	char *p, *line;
	int nifs = 0, total = 0;
	if (run_capture("iw dev", out, sizeof(out), 1800) < 0) return -1;
	line = strtok_r(out, "\n", &p);
	while (line && nifs < 128) {
		char name[IFNAMSIZ];
		if (sscanf(line, " Interface %15s", name) == 1) {
			int good = 1; size_t i;
			for (i = 0; name[i]; i++)
				if (!(isalnum((unsigned char)name[i]) || name[i] == '_' ||
				      name[i] == '-' || name[i] == '.')) good = 0;
			if (good) strcpy(ifaces[nifs++], name);
		}
		line = strtok_r(NULL, "\n", &p);
	}
	for (int i = 0; i < nifs; i++) {
		snprintf(cmd, sizeof(cmd), "iw dev %s station dump", ifaces[i]);
		if (run_capture(cmd, out, sizeof(out), 1800) < 0) continue;
		line = strtok_r(out, "\n", &p);
		while (line) { if (!strncmp(line, "Station ", 8)) total++; line = strtok_r(NULL, "\n", &p); }
	}
	return total;
}

static int run_capture(const char *command, char *out, size_t cap, int timeout_ms)
{
	int fd[2], status = 0, elapsed = 0, done = 0;
	pid_t pid;
	size_t used = 0;
	if (pipe(fd) < 0) return -1;
	pid = fork();
	if (pid < 0) { close(fd[0]); close(fd[1]); return -1; }
	if (!pid) {
		int nullfd = open("/dev/null", O_WRONLY);
		setpgid(0, 0); close(fd[0]); dup2(fd[1], STDOUT_FILENO); close(fd[1]);
		if (nullfd >= 0) { dup2(nullfd, STDERR_FILENO); close(nullfd); }
		execl("/bin/sh", "sh", "-c", command, (char *)NULL);
		_exit(127);
	}
	close(fd[1]); setpgid(pid, pid);
	(void)fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | O_NONBLOCK);
	while (elapsed < timeout_ms) {
		struct pollfd pfd = { .fd = fd[0], .events = POLLIN | POLLHUP };
		int pr = poll(&pfd, 1, 50);
		if (pr > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
			for (;;) {
				char discard[512];
				void *dst = cap > used + 1 ? (void *)(out + used) : (void *)discard;
				size_t room = cap > used + 1 ? cap - used - 1 : sizeof(discard);
				ssize_t nr = read(fd[0], dst, room);
				if (nr > 0) {
					if (cap > used + 1) used += (size_t)nr;
				} else {
					break;
				}
			}
		}
		if (waitpid(pid, &status, WNOHANG) == pid) { done = 1; break; }
		elapsed += 50;
	}
	if (!done) { kill(-pid, SIGKILL); waitpid(pid, &status, 0); }
	close(fd[0]);
	out[used < cap ? used : cap - 1] = '\0';
	return done && WIFEXITED(status) && WEXITSTATUS(status) == 0 ? (int)used : -1;
}

static int ip_for_iface(const char *iface, char *out, size_t cap)
{
	int fd;
	struct ifreq req;
	struct sockaddr_in *sin;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return -1;
	memset(&req, 0, sizeof(req));
	snprintf(req.ifr_name, sizeof(req.ifr_name), "%s", iface);
	if (ioctl(fd, SIOCGIFADDR, &req) < 0) { close(fd); return -1; }
	sin = (struct sockaddr_in *)&req.ifr_addr;
	if (!inet_ntop(AF_INET, &sin->sin_addr, out, cap)) { close(fd); return -1; }
	close(fd); return 0;
}

static int default_iface(char *out, size_t cap)
{
	FILE *f = fopen("/proc/net/route", "r");
	char line[256], iface[IFNAMSIZ], dest[32]; unsigned long gateway;
	unsigned int flags;
	if (!f) return -1;
	if (!fgets(line, sizeof(line), f)) { fclose(f); return -1; }
	while (fgets(line, sizeof(line), f)) {
		if (sscanf(line, "%15s %31s %lx %x", iface, dest, &gateway, &flags) == 4 &&
		    !strcmp(dest, "00000000") && (flags & 1)) {
			snprintf(out, cap, "%s", iface); fclose(f); return 0;
		}
	}
	fclose(f); return -1;
}

static int source_value(const char *source, char *out, size_t cap)
{
	char buf[8192], path[256], iface[IFNAMSIZ];
	if (!strcmp(source, "time")) {
		time_t now = time(NULL); struct tm tmv;
		localtime_r(&now, &tmv); strftime(out, cap, "%H:%M:%S", &tmv); return 0;
	}
	if (!strcmp(source, "uptime")) {
		if (read_small("/proc/uptime", buf, sizeof(buf)) < 0) return -1;
		double sec = atof(buf); snprintf(out, cap, "%dd %02dh %02dm",
			(int)(sec / 86400), ((int)sec / 3600) % 24, ((int)sec / 60) % 60); return 0;
	}
	if (!strcmp(source, "lan_ip"))
		return ip_for_iface("br-lan", out, cap);
	if (!strcmp(source, "wan_ip")) {
		if (default_iface(iface, sizeof(iface)) < 0) return -1;
		return ip_for_iface(iface, out, cap);
	}
	if (!strcmp(source, "wifi_clients")) {
		int count = wifi_clients(); if (count < 0) return -1;
		snprintf(out, cap, "%d", count); return 0;
	}
	if (!strcmp(source, "cpu_load")) {
		if (read_small("/proc/loadavg", buf, sizeof(buf)) < 0) return -1;
		char *sp = strchr(buf, ' '); if (sp) *sp = 0;
		snprintf(out, cap, "%s", buf); return 0;
	}
	if (!strcmp(source, "mem_free")) {
		FILE *f = fopen("/proc/meminfo", "r"); unsigned long kb = 0;
		if (!f) return -1;
		while (fgets(buf, sizeof(buf), f))
			if (sscanf(buf, "MemAvailable: %lu kB", &kb) == 1) break;
		fclose(f); if (!kb) return -1;
		snprintf(out, cap, "%lu MB", kb / 1024); return 0;
	}
	if (!strcmp(source, "battery")) {
		DIR *d = opendir("/sys/class/power_supply"); struct dirent *de;
		if (!d) return -1;
		while ((de = readdir(d))) {
			if (de->d_name[0] == '.') continue;
			snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type", de->d_name);
			if (read_small(path, buf, sizeof(buf)) < 0 || strcasecmp(buf, "Battery")) continue;
			snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", de->d_name);
			if (!read_small(path, buf, sizeof(buf))) {
				snprintf(out, cap, "%s%%", buf); closedir(d); return 0;
			}
		}
		closedir(d); return -1;
	}
	if (!strncmp(source, "exec:", 5) && source[5]) {
		if (run_capture(source + 5, out, cap, 2500) < 0) return -1;
		trim(out);
		for (size_t i = 0; out[i]; i++)
			if (iscntrl((unsigned char)out[i])) out[i] = ' ';
		return 0;
	}
	snprintf(out, cap, "--"); return -1;
}

static int backlight_set(int value)
{
	DIR *d = opendir("/sys/class/backlight");
	struct dirent *de;
	char path[512], maxpath[512], buf[64];
	int max = 255, fd, n;
	if (!d || value < 0) { if (d) closedir(d); return 0; }
	while ((de = readdir(d))) {
		if (de->d_name[0] == '.') continue;
		snprintf(path, sizeof(path), "/sys/class/backlight/%s/brightness", de->d_name);
		snprintf(maxpath, sizeof(maxpath), "/sys/class/backlight/%s/max_brightness", de->d_name);
		if (!read_small(maxpath, buf, sizeof(buf))) max = atoi(buf);
		if (value > max) value = max;
		fd = open(path, O_WRONLY);
		if (fd >= 0) {
			ssize_t written;
			n = snprintf(buf, sizeof(buf), "%d\n", value);
			written = write(fd, buf, (size_t)n);
			if (written < 0 || written != n) {
				close(fd); closedir(d); return -1;
			}
			close(fd);
		}
		closedir(d); return 0;
	}
	closedir(d); return 0;
}

static int render(uint8_t *frame, int w, int h, int stride,
                  const struct settings *s)
{
	uint16_t bg = color565(s->bg, 0), fg = color565(s->fg, 0xffff);
	int scale = s->font_scale, step = 10 * scale, y = 0;
	fill_rect(frame, w, h, stride, s->rotation, 0, 0, w, h, bg);
	for (size_t i = 0; i < s->nlines && y + 8 * scale <= h; i++, y += step) {
		char value[MAX_TEXT] = "", text[MAX_TEXT + MAX_LABEL + 4];
		uint16_t lc = color565(s->lines[i].color, fg);
		if (!s->lines[i].label[0] && !s->lines[i].source[0]) continue;
		if (source_value(s->lines[i].source, value, sizeof(value)) < 0 && !value[0])
			strcpy(value, "--");
		snprintf(text, sizeof(text), "%s%s%s", s->lines[i].label,
		         s->lines[i].label[0] ? ": " : "", value);
		draw_text(frame, w, h, stride, s->rotation, 0, y, text, lc, bg, scale);
	}
	return 0;
}

static int write_frame(int fd, const uint8_t *frame, size_t bytes)
{
	ssize_t n;
	if (lseek(fd, 0, SEEK_SET) < 0) return -1;
	n = write(fd, frame, bytes);
	return n == (ssize_t)bytes ? 0 : -1;
}

static int run(const char *fbpath, int test_mode)
{
	struct settings s;
	const char *cfg = getenv("LCDSTATUS_CONFIG");
	struct fb_var_screeninfo v;
	struct fb_fix_screeninfo f;
	int fd = -1, w, h, stride, scale, result = 0;
	size_t bytes;
	uint8_t *frame;
	if (!cfg) cfg = DEFAULT_CONFIG;
	if (load_settings(&s, cfg) < 0) { perror("lcdstatusd: config"); return 1; }
	if (!s.enabled && !test_mode) return 0;
	if (test_mode) {
		w = 128; h = 128; stride = w * 2;
		fd = open(fbpath, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	} else {
		fd = open(fbpath, O_RDWR | O_CLOEXEC);
	}
	if (fd < 0) { perror("lcdstatusd: framebuffer"); return 1; }
	if (!test_mode) {
		if (ioctl(fd, FBIOGET_VSCREENINFO, &v) < 0 ||
		    ioctl(fd, FBIOGET_FSCREENINFO, &f) < 0) {
			perror("lcdstatusd: framebuffer info"); close(fd); return 1;
		}
		if (v.bits_per_pixel != 16 || v.red.offset != 11 || v.red.length != 5 ||
		    v.green.offset != 5 || v.green.length != 6 ||
		    v.blue.offset != 0 || v.blue.length != 5 || v.grayscale) {
			fprintf(stderr, "lcdstatusd: unsupported framebuffer format (requires RGB565)\n");
			close(fd); return 1;
		}
		w = (int)v.xres; h = (int)v.yres; stride = (int)f.line_length;
		if (w <= 0 || h <= 0 || stride < w * 2 ||
		    f.smem_len < (unsigned long)stride * (unsigned long)h) {
			fprintf(stderr, "lcdstatusd: invalid framebuffer geometry\n");
			close(fd); return 1;
		}
	}
	bytes = (size_t)stride * (size_t)h;
	frame = calloc(1, bytes);
	if (!frame) { perror("lcdstatusd: frame"); close(fd); return 1; }
	s.rotation = s.rotation == 180 ? 180 : 0;
	scale = s.font_scale;
	if (!test_mode) backlight_set(s.backlight);
	if (test_mode) {
		render(frame, w, h, stride, &s);
		if (write_frame(fd, frame, bytes) < 0) { perror("lcdstatusd: write"); result = 1; }
		else fprintf(stderr, "lcdstatusd: rendered test frame %dx%d RGB565 (%zu bytes)\n", w, h, bytes);
	} else {
		while (!stopping) {
			struct timespec ts;
			render(frame, w, h, stride, &s);
			if (write_frame(fd, frame, bytes) < 0) { perror("lcdstatusd: framebuffer write"); result = 1; break; }
			ts.tv_sec = s.refresh_s; ts.tv_nsec = 0;
			while (!stopping && nanosleep(&ts, &ts) < 0 && errno == EINTR) {}
		}
	}
	(void)scale;
	free(frame); close(fd); return result;
}

int main(int argc, char **argv)
{
	const char *path = "/dev/fb0";
	int test = 0;
	if (argc == 3 && !strcmp(argv[1], "--test-fb")) { path = argv[2]; test = 1; }
	else if (argc > 1 && strcmp(argv[1], "--help")) {
		fprintf(stderr, "usage: lcdstatusd [--test-fb FILE]\n"); return 2;
	} else if (argc > 1) {
		printf("usage: lcdstatusd [--test-fb FILE]\n"); return 0;
	}
	signal(SIGTERM, on_signal); signal(SIGINT, on_signal);
	return run(path, test);
}
