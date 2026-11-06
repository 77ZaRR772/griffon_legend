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

#ifndef _CONFIG_H_
#define _CONFIG_H_

typedef struct {
	int showdbginfo;
	int showlogos;
	int scr_width;
	int scr_height;
	int scr_bpp;
	int fullscreen;
	int stretch;
	int fastfade;
	int hwaccel;
	int hwsurface;
	int music;
	int musicvol;
	int effects;
	int effectsvol;
	int random_enemies;
	int key_up;
	int key_down;
	int key_left;
	int key_right;
	int key_attack;
	int key_item;
	int key_menu;
	int key_dbg;
	int key_cheat_god;
	int key_cheat_noclip;
	int key_spell1;
	int key_spell2;
	int key_spell3;
	int key_spell4;
	int key_spell5;
} CONFIG;

extern CONFIG config;
extern char config_ini[];

void config_defaults(CONFIG *config);
void config_load(CONFIG *config);
void config_save(CONFIG *config);

#endif // _CONFIG_H_
