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

CONFIG config;
char config_ini[64] = "config.ini";

#define CFG_BOOL 0
#define CFG_INT  1
#define CFG_FLAG 2
#define CFG_KEY  3

typedef struct {
	const char *name;
	int type;
	int offset;
	int flag_val;
	int default_val;
	int min;
	int max;
} ConfigEntry;

static const ConfigEntry entries[] = {
	{"showdbginfo",    CFG_BOOL, offsetof(CONFIG, showdbginfo),    0,             0,           0, 0},
	{"showlogos",      CFG_BOOL, offsetof(CONFIG, showlogos),      0,             0,           0, 0},
	{"scr_width",      CFG_INT,  offsetof(CONFIG, scr_width),      0,             320,         0, 0},
	{"scr_height",     CFG_INT,  offsetof(CONFIG, scr_height),     0,             240,         0, 0},
	{"scr_bits",       CFG_INT,  offsetof(CONFIG, scr_bpp),        0,             32,          0, 0},
	{"hwaccel",        CFG_FLAG, offsetof(CONFIG, hwaccel),        SDL_HWACCEL,   0,           0, 0},
	{"hwsurface",      CFG_FLAG, offsetof(CONFIG, hwsurface),      SDL_HWSURFACE, 0,           0, 0},
	{"fullscreen",     CFG_FLAG, offsetof(CONFIG, fullscreen),     SDL_FULLSCREEN, 0,          0, 0},
	{"stretch",        CFG_BOOL, offsetof(CONFIG, stretch),        0,             0,           0, 0},
	{"fastfade",       CFG_BOOL, offsetof(CONFIG, fastfade),       0,             0,           0, 0},
	{"music",          CFG_BOOL, offsetof(CONFIG, music),          0,             1,           0, 0},
	{"sndeffects",     CFG_BOOL, offsetof(CONFIG, effects),        0,             1,           0, 0},
	{"opmusicvol",     CFG_INT,  offsetof(CONFIG, musicvol),       0,             127,         0, 255},
	{"opeffectsvol",   CFG_INT,  offsetof(CONFIG, effectsvol),     0,             127,         0, 255},
	{"random_enemies", CFG_BOOL, offsetof(CONFIG, random_enemies), 0,             0,           0, 0},
	{"key_up",         CFG_KEY,  offsetof(CONFIG, key_up),         0,             SDLK_UP,     0, SDLK_LAST - 1},
	{"key_down",       CFG_KEY,  offsetof(CONFIG, key_down),       0,             SDLK_DOWN,   0, SDLK_LAST - 1},
	{"key_left",       CFG_KEY,  offsetof(CONFIG, key_left),       0,             SDLK_LEFT,   0, SDLK_LAST - 1},
	{"key_right",      CFG_KEY,  offsetof(CONFIG, key_right),      0,             SDLK_RIGHT,  0, SDLK_LAST - 1},
	{"key_attack",     CFG_KEY,  offsetof(CONFIG, key_attack),     0,             SDLK_LCTRL,  0, SDLK_LAST - 1},
	{"key_item",       CFG_KEY,  offsetof(CONFIG, key_item),       0,             SDLK_LALT,   0, SDLK_LAST - 1},
	{"key_menu",       CFG_KEY,  offsetof(CONFIG, key_menu),       0,             SDLK_ESCAPE, 0, SDLK_LAST - 1},
	{"key_dbg",        CFG_KEY,  offsetof(CONFIG, key_dbg),        0,             SDLK_F12,    0, SDLK_LAST - 1},
	{"key_cheat_god",  CFG_KEY,  offsetof(CONFIG, key_cheat_god),  0,             SDLK_F11,    0, SDLK_LAST - 1},
	{"key_cheat_noclip", CFG_KEY, offsetof(CONFIG, key_cheat_noclip), 0,           SDLK_F10,    0, SDLK_LAST - 1},
	{"key_spell1",     CFG_KEY,  offsetof(CONFIG, key_spell1),       0,           SDLK_1,      0, SDLK_LAST - 1},
	{"key_spell2",     CFG_KEY,  offsetof(CONFIG, key_spell2),       0,           SDLK_2,      0, SDLK_LAST - 1},
	{"key_spell3",     CFG_KEY,  offsetof(CONFIG, key_spell3),       0,           SDLK_3,      0, SDLK_LAST - 1},
	{"key_spell4",     CFG_KEY,  offsetof(CONFIG, key_spell4),       0,           SDLK_4,      0, SDLK_LAST - 1},
	{"key_spell5",     CFG_KEY,  offsetof(CONFIG, key_spell5),       0,           SDLK_5,      0, SDLK_LAST - 1},
};
#define NUM_ENTRIES (sizeof(entries) / sizeof(entries[0]))

static int *config_value(CONFIG *cfg, const ConfigEntry *entry) {
	return (int *)((char *)cfg + entry->offset);
}

void config_defaults(CONFIG *cfg) {
	for(size_t i = 0; i < NUM_ENTRIES; i++)
		*config_value(cfg, &entries[i]) = entries[i].default_val;
}

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

static void config_validate(CONFIG *cfg) {
	if(!((cfg->scr_width == 320 && cfg->scr_height == 240) ||
		(cfg->scr_width == 640 && cfg->scr_height == 480))) {
		cfg->scr_width = 320;
		cfg->scr_height = 240;
	}
	if(cfg->scr_bpp != 16 && cfg->scr_bpp != 24 && cfg->scr_bpp != 32) cfg->scr_bpp = 32;

	for(size_t i = 0; i < NUM_ENTRIES; i++) {
		const ConfigEntry *entry = &entries[i];
		int *value = config_value(cfg, entry);

		if(entry->type == CFG_KEY && (*value < 0 || *value >= SDLK_LAST))
			*value = entry->default_val;
		if(entry->type == CFG_INT && entry->max > entry->min) {
			if(*value < entry->min) *value = entry->min;
			if(*value > entry->max) *value = entry->max;
		}
	}
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
				int *ptr = config_value(cfg, &entries[i]);
				switch (entries[i].type) {
					case CFG_INT:
					case CFG_KEY:  *ptr = atoi(val); break;
					case CFG_BOOL: *ptr = parse_bool(val); break;
					case CFG_FLAG: *ptr = parse_bool(val) ? entries[i].flag_val : 0; break;
				}
				break;
			}
		}
	}
	fclose(fp);
	config_validate(cfg);
}

void config_save(CONFIG *cfg) {
	FILE *fp = fopen(config_ini, "w");
	if (!fp) return;

	for (size_t i = 0; i < NUM_ENTRIES; i++) {
		int *ptr = config_value(cfg, &entries[i]);
		if (entries[i].type == CFG_INT || entries[i].type == CFG_KEY) {
			fprintf(fp, "%s:%d\n", entries[i].name, *ptr);
		} else {
			int v = (entries[i].type == CFG_BOOL) ? *ptr : (*ptr != 0);
			fprintf(fp, "%s:%s\n", entries[i].name, v ? "YES" : "NO");
		}
	}
	fclose(fp);
}
