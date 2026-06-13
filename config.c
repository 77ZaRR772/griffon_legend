/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <SDL.h>
#include "config.h"

CONFIG config = {0, 0, 320, 240, 16, 0, 0, 0, 1, 127, 1, 127};
char config_ini[64] = "config.ini";

#define CFG_BOOL 0
#define CFG_INT  1
#define CFG_FLAG 2	//sdl_flag

typedef struct {
	const char *name;
	int type;
	int offset;
	int flag_val;
} ConfigEntry;

static const ConfigEntry entries[] = {
	{"showdbginfo",  CFG_BOOL, offsetof(CONFIG, showdbginfo), 0},
	{"showlogos",    CFG_BOOL, offsetof(CONFIG, showlogos),   0},
	{"scr_width",    CFG_INT,  offsetof(CONFIG, scr_width),   0},
	{"scr_height",   CFG_INT,  offsetof(CONFIG, scr_height),  0},
	{"scr_bits",     CFG_INT,  offsetof(CONFIG, scr_bpp),     0},
	{"hwaccel",      CFG_FLAG, offsetof(CONFIG, hwaccel),     SDL_HWACCEL},
	{"hwsurface",    CFG_FLAG, offsetof(CONFIG, hwsurface),   SDL_HWSURFACE},
	{"fullscreen",   CFG_FLAG, offsetof(CONFIG, fullscreen),  SDL_FULLSCREEN},
	{"music",        CFG_BOOL, offsetof(CONFIG, music),       0},
	{"sndeffects",   CFG_BOOL, offsetof(CONFIG, effects),     0},
	{"opmusicvol",   CFG_INT,  offsetof(CONFIG, musicvol),    0},
	{"opeffectsvol", CFG_INT,  offsetof(CONFIG, effectsvol),  0},
};
#define NUM_ENTRIES (sizeof(entries) / sizeof(entries[0]))

static int str_casecmp(const char *a, const char *b) {
	while (*a && *b) {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 1;
		a++; b++;
	}
	return *a != *b;
}

static int parse_bool(const char *s) {
	return (strcmp(s, "1") == 0 ||
	str_casecmp(s, "yes") == 0 ||
	str_casecmp(s, "true") == 0 ||
	str_casecmp(s, "on") == 0);
}

static void trim_right(char *s) {
	int len = strlen(s);
	while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' ||
		s[len-1] == ' '  || s[len-1] == '\t'))
		s[--len] = '\0';
}

void config_load(CONFIG *cfg) {
	FILE *fp = fopen(config_ini, "r");
	if (!fp) return;

	char line[256];
	while (fgets(line, sizeof(line), fp)) {
		char *sep = line;
		while (*sep && *sep != ':' && *sep != ' ' && *sep != '\t' &&
			*sep != '\n' && *sep != '\r') sep++;

		if (*sep == '\0' || *sep == '\n' || *sep == '\r') continue;

		*sep = '\0';
		char *val = sep + 1;

		while (*val == ':' || *val == ' ' || *val == '\t') val++;
		trim_right(val);

		char valbuf[128];
		if (*val == '\0' && fgets(valbuf, sizeof(valbuf), fp)) {
			char *v = valbuf;
			while (*v == ' ' || *v == '\t' || *v == ':') v++;
			trim_right(v);
			val = v;
		}

		for (char *p = line; *p; p++) *p = tolower((unsigned char)*p);

		for (size_t i = 0; i < NUM_ENTRIES; i++) {
			if (strcmp(line, entries[i].name) == 0) {
				int *ptr = (int *)((char *)cfg + entries[i].offset);
				switch (entries[i].type) {
					case CFG_INT:  *ptr = atoi(val); break;
					case CFG_BOOL: *ptr = parse_bool(val); break;
					case CFG_FLAG: *ptr = parse_bool(val) ? entries[i].flag_val : 0; break;
				}
				break;
			}
		}
	}
	fclose(fp);
}

void config_save(CONFIG *cfg) {
	FILE *fp = fopen(config_ini, "w");
	if (!fp) return;

	for (size_t i = 0; i < NUM_ENTRIES; i++) {
		int *ptr = (int *)((char *)cfg + entries[i].offset);
		if (entries[i].type == CFG_INT) {
			fprintf(fp, "%s:%d\n", entries[i].name, *ptr);
		} else {
			int v = (entries[i].type == CFG_BOOL) ? *ptr : (*ptr != 0);
			fprintf(fp, "%s:%s\n", entries[i].name, v ? "YES" : "NO");
		}
	}
	fclose(fp);
}
