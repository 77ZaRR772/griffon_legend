#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "griffon.h"
#include "editor.h"

extern SDL_Surface *video, *videobuffer;
extern SDL_Surface *tiles[4], *anims[100];
extern int objecttile[33][9][3][3][2], objectinfo[33][6];
extern int ticks, itemticks;
extern int triggers[10000][9];

void game_setpause(int paused);
void sdl_blitscale(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
int sys_pollevent(void);

static void editor_present(void)
{
#ifdef OPENDINGUX
	SDL_BlitSurface(videobuffer, NULL, video, NULL);
#else
	sdl_blitscale(videobuffer, NULL, video, NULL);
#endif
	SDL_Flip(video);
}

static int editor_save(int mapnum, int data[320][200], char *status)
{
	char name[64], backup[68], temporary[68];
	FILE *fp;

	sprintf(name, "mapdb/%04i.map", mapnum);
	sprintf(backup, "%s.bak", name);
	sprintf(temporary, "%s.tmp", name);

	fp = fopen(temporary, "w");
	if(!fp) {
		strcpy(status, "SAVE FAILED");
		return 0;
	}
	for(int x = 0; x < 320; x++) {
		for(int y = 0; y < 200; y++) fprintf(fp, " %i \n", data[x][y]);
	}
	if(fclose(fp) != 0) {
		remove(temporary);
		strcpy(status, "SAVE FAILED");
		return 0;
	}

	remove(backup);
	if(rename(name, backup) != 0) {
		remove(temporary);
		strcpy(status, "BACKUP FAILED");
		return 0;
	}
	if(rename(temporary, name) != 0) {
		rename(backup, name);
		remove(temporary);
		strcpy(status, "SAVE FAILED");
		return 0;
	}

	strcpy(status, "SAVED");
	return 1;
}

static int editor_loadnpc(int mapnum, int npcdata[MAXNPC][11])
{
	char name[64];
	FILE *fp;

	sprintf(name, "mapdb/%04i.npc", mapnum);
	fp = fopen(name, "r");
	if(!fp) return 0;
	for(int i = 0; i < MAXNPC; i++) {
		for(int j = 0; j < 11; j++) {
			if(fscanf(fp, "%i", &npcdata[i][j]) != 1) {
				fclose(fp);
				return 0;
			}
		}
	}
	fclose(fp);
	return 1;
}

static int editor_loadtriggers(int mapnum, int triggerdata[20][15])
{
	char name[64];
	int count, x, y, trigger;
	FILE *fp;

	for(x = 0; x < 20; x++)
		for(y = 0; y < 15; y++) triggerdata[x][y] = -1;
	sprintf(name, "mapdb/%04i.trg", mapnum);
	fp = fopen(name, "r");
	if(!fp) return 0;
	if(fscanf(fp, "%i", &count) != 1 || count < 0 || count > 10000) {
		fclose(fp);
		return 0;
	}
	for(int i = 0; i < count; i++) {
		if(fscanf(fp, "%i %i %i", &x, &y, &trigger) != 3 || x < 0 || x >= 20 || y < 0 || y >= 15) {
			fclose(fp);
			return 0;
		}
		triggerdata[x][y] = trigger;
	}
	fclose(fp);
	return 1;
}

static int editor_savetriggers(int mapnum, int triggerdata[20][15], char *status)
{
	char name[64], backup[68], temporary[68];
	FILE *fp;

	sprintf(name, "data/triggers.dat");
	sprintf(backup, "%s.bak", name);
	sprintf(temporary, "%s.tmp", name);
	fp = fopen(temporary, "w");
	if(!fp) {
		strcpy(status, "TRIGGER SAVE FAILED");
		return 0;
	}
	for(int i = 0; i < 10000; i++)
		for(int j = 0; j < 9; j++) fprintf(fp, " %i \n", triggers[i][j]);
	remove(backup);
	if(fclose(fp) != 0 || rename(name, backup) != 0 || rename(temporary, name) != 0) {
		remove(temporary);
		strcpy(status, "TRIGGER SAVE FAILED");
		return 0;
	}

	sprintf(name, "mapdb/%04i.trg", mapnum);
	sprintf(backup, "%s.bak", name);
	sprintf(temporary, "%s.tmp", name);
	fp = fopen(temporary, "w");
	if(!fp) {
		strcpy(status, "TRIGGER SAVE FAILED");
		return 0;
	}
	int count = 0;
	for(int x = 0; x < 20; x++)
		for(int y = 0; y < 15; y++) if(triggerdata[x][y] >= 0) count++;
	fprintf(fp, " %i \n", count);
	for(int x = 0; x < 20; x++)
		for(int y = 0; y < 15; y++)
			if(triggerdata[x][y] >= 0) fprintf(fp, " %i \n %i \n %i \n", x, y, triggerdata[x][y]);
	remove(backup);
	if(fclose(fp) != 0 || rename(name, backup) != 0 || rename(temporary, name) != 0) {
		remove(temporary);
		strcpy(status, "TRIGGER SAVE FAILED");
		return 0;
	}
	return 1;
}

static int editor_savenpc(int mapnum, int npcdata[MAXNPC][11], char *status)
{
	char name[64], backup[68], temporary[68];
	FILE *fp;

	sprintf(name, "mapdb/%04i.npc", mapnum);
	sprintf(backup, "%s.bak", name);
	sprintf(temporary, "%s.tmp", name);
	fp = fopen(temporary, "w");
	if(!fp) {
		strcpy(status, "NPC SAVE FAILED");
		return 0;
	}
	for(int i = 0; i < MAXNPC; i++) {
		for(int j = 0; j < 11; j++) fprintf(fp, " %i \n", npcdata[i][j]);
	}
	if(fclose(fp) != 0) {
		remove(temporary);
		strcpy(status, "NPC SAVE FAILED");
		return 0;
	}
	remove(backup);
	if(rename(name, backup) != 0) {
		remove(temporary);
		strcpy(status, "NPC SAVE FAILED");
		return 0;
	}
	if(rename(temporary, name) != 0) {
		rename(backup, name);
		remove(temporary);
		strcpy(status, "NPC SAVE FAILED");
		return 0;
	}
	return 1;
}

static int editor_layer, editor_tileset, editor_tile = 1, editor_collision;
static int editor_entitymode, editor_entitykind, editor_entity = 1, editor_showentities = 1;
static int editor_keypause;
static int cursorx, cursory, camerax, cameray;

static void editor_outline(int x, int y, Uint32 color)
{
	SDL_Rect rect;

	rect.x = x;
	rect.y = y;
	rect.w = 16;
	rect.h = 1;
	SDL_FillRect(videobuffer, &rect, color);
	rect.y += 15;
	SDL_FillRect(videobuffer, &rect, color);
	rect.y = y;
	rect.w = 1;
	rect.h = 16;
	SDL_FillRect(videobuffer, &rect, color);
	rect.x += 15;
	SDL_FillRect(videobuffer, &rect, color);
}

static void editor_drawmonster(int type, int x, int y)
{
	SDL_Rect src = {0, 24, 24, 24};
	SDL_Rect dst = {x - 4, y - 5, 24, 24};

	if(type == 2 || type == 9) {
		src.x = 0;
		src.y = 0;
		src.w = 42;
		src.h = 36;
		dst.x = x - 8;
		dst.y = y - 2;
		SDL_BlitSurface(anims[type], &src, videobuffer, &dst);
		return;
	}
	if(type == 5) {
		src.y = 0;
		src.w = 42;
		src.h = 36;
		dst.x = x - 9;
		dst.y = y - 18;
		SDL_BlitSurface(anims[type], &src, videobuffer, &dst);
		return;
	}
	if(type == 3 || type == 4) {
		src.y = 0;
		src.w = 24;
		src.h = 48;
		dst.x = x - 6;
		dst.y = y - 29;
	} else if(type == 10) {
		src.x = 74;
		src.y = 0;
		src.w = 74;
		src.h = 48;
		dst.x = x - 29;
		dst.y = y - 21;
	} else if(type == 11) {
		src.y = 0;
		src.w = 80;
		src.h = 72;
		dst.x = x - 28;
		dst.y = y - 38;
	} else if(type == 12) {
		src.y = 0;
		src.w = 99;
		src.h = 80;
		dst.x = x - 38;
		dst.y = y - 48;
	}
	SDL_BlitSurface(anims[type], &src, videobuffer, &dst);
}

static void editor_drawobject(int object, int x, int y)
{
	for(int ox = 0; ox < objectinfo[object][1]; ox++) {
		for(int oy = 0; oy < objectinfo[object][2]; oy++) {
			int value = objecttile[object][0][ox][oy][0];
			if(value <= 0) continue;
			SDL_Rect src = {(value - 1) % 20 * 16, (value - 1) / 20 * 16, 16, 16};
			SDL_Rect dst = {x + ox * 16, y + oy * 16, 16, 16};
			SDL_BlitSurface(tiles[3], &src, videobuffer, &dst);
		}
	}
}

static void editor_picktile(void)
{
	if(editor_tileset > 2) editor_tileset = 0;
	int selected = editor_tile - 1;
	int rowoffset = 0;
	editor_keypause = ticks + 200;

	for(;;) {
		int columns = tiles[editor_tileset]->w / 16;
		int count = columns * (tiles[editor_tileset]->h / 16);
		int rows = (count + columns - 1) / columns;
		if(selected >= count) selected = count - 1;
		if(selected / columns < rowoffset) rowoffset = selected / columns;
		if(selected / columns >= rowoffset + 11) rowoffset = selected / columns - 10;
		if(rowoffset > rows - 11) rowoffset = rows > 11 ? rows - 11 : 0;

		SDL_FillRect(videobuffer, NULL, 0);
		SDL_Rect src = {0, rowoffset * 16, tiles[editor_tileset]->w, 176};
		SDL_Rect dst = {0, 0, 320, 176};
		SDL_BlitSurface(tiles[editor_tileset], &src, videobuffer, &dst);
		editor_outline(selected % columns * 16, (selected / columns - rowoffset) * 16,
			SDL_MapRGB(videobuffer->format, 255, 255, 0));

		game_fillrect(videobuffer, 0, 208, 320, 32, 0);
		char line[96];
		sprintf(line, "TILESET %i  TILE %03i  LAYER %i", editor_tileset, selected + 1, editor_layer + 1);
		sys_print(videobuffer, line, 4, 210, 0);
		sys_print(videobuffer, "ARROWS SELECT 1-3 TILESET PGUP/DN LAYER ENTER OK", 4, 220, 0);
		sys_print(videobuffer, "T/ESC CANCEL", 4, 230, 0);
		editor_present();
		sys_pollevent();

		if(event.type == SDL_KEYDOWN && editor_keypause < ticks) {
			int key = event.key.keysym.sym;
			editor_keypause = ticks + 100;
			if(key == SDLK_ESCAPE || key == SDLK_t) return;
			if(key == SDLK_LEFT && selected > 0) selected--;
			if(key == SDLK_RIGHT && selected + 1 < count) selected++;
			if(key == SDLK_UP && selected >= columns) selected -= columns;
			if(key == SDLK_DOWN && selected + columns < count) selected += columns;
			if(key >= SDLK_1 && key <= SDLK_3) {
				editor_tileset = key - SDLK_1;
				selected = 0;
				rowoffset = 0;
			}
			if(key == SDLK_LEFTBRACKET || key == SDLK_PAGEDOWN) editor_layer = (editor_layer + 2) % 3;
			if(key == SDLK_RIGHTBRACKET || key == SDLK_PAGEUP) editor_layer = (editor_layer + 1) % 3;
			if(key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_LCTRL || key == SDLK_RCTRL) {
				editor_tile = selected + 1;
				return;
			}
		}
		sys_tickcalc();
	}
}

static void editor_pickentity(void)
{
	static const char *names[12] = {
		"BABY DRAGON", "ONE WING", "BOSS", "BLACK KNIGHT",
		"FIRE HYDRA", "RED DRAGON", "PRIEST", "YELLOW DRAGON",
		"TWO WING", "DRAGON", "FINAL BOSS", "BAT KITTY"
	};
	int selected = editor_entitykind ? editor_entity : editor_entity - 1;
	editor_keypause = ticks + 200;

	for(;;) {
		int count = editor_entitykind ? 21 : 12;
		game_fillrect(videobuffer, 64, 48, 192, 144, 0);
		sys_print(videobuffer, editor_entitykind ? "SELECT OBJECT" : "SELECT MONSTER", 88, 58, 3);
		char line[64];
		if(editor_entitykind) sprintf(line, "OBJECT %02i", selected);
		else sprintf(line, "%s", names[selected]);
		sys_print(videobuffer, line, 88, 82, 4);
		sys_print(videobuffer, "ARROWS: SELECT", 88, 116, 0);
		sys_print(videobuffer, "TAB: MONSTER/OBJECT", 88, 128, 0);
		sys_print(videobuffer, "ENTER/SPACE: OK", 88, 140, 0);
		sys_print(videobuffer, "E/ESC: CANCEL", 88, 152, 0);

		if(editor_entitykind) editor_drawobject(selected, 152, 92);
		else editor_drawmonster(selected + 1, 152, 92);
		editor_present();
		sys_pollevent();

		if(event.type == SDL_KEYDOWN && editor_keypause < ticks) {
			int key = event.key.keysym.sym;
			editor_keypause = ticks + 150;
			if(key == SDLK_ESCAPE || key == SDLK_e) return;
			if((key == SDLK_LEFT || key == SDLK_UP) && selected > 0) selected--;
			if((key == SDLK_RIGHT || key == SDLK_DOWN) && selected + 1 < count) selected++;
			if(key == SDLK_TAB) {
				editor_entitykind = !editor_entitykind;
				selected = 0;
			}
			if(key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_LCTRL || key == SDLK_RCTRL) {
				editor_entity = editor_entitykind ? selected : selected + 1;
				editor_entitymode = 1;
				return;
			}
		}
		sys_tickcalc();
	}
}

static void editor_placeentity(int data[320][200], int npcdata[MAXNPC][11],
	int x, int y, char *status)
{
	if(x >= 20 || y >= 15) {
		strcpy(status, "ENTITY AREA: 20X15");
		return;
	}

	int *flags = &data[120 + x][y];
	if(editor_entitykind) {
		data[200 + x][y] = editor_entity;
		*flags |= 4;
		strcpy(status, "OBJECT PLACED");
		return;
	}

	int slot = -1;
	if(*flags & 2) slot = data[160 + x][y];
	else {
		for(int candidate = 1; candidate < MAXNPC && slot < 0; candidate++) {
			int used = 0;
			for(int mx = 0; mx < 20 && !used; mx++)
				for(int my = 0; my < 15 && !used; my++)
					if((data[120 + mx][my] & 2) && data[160 + mx][my] == candidate) used = 1;
			if(!used) slot = candidate;
		}
	}
	if(slot < 0 || slot >= MAXNPC) {
		strcpy(status, "NO FREE NPC SLOTS");
		return;
	}

	memset(npcdata[slot], 0, sizeof(npcdata[slot]));
	npcdata[slot][0] = editor_entity;
	npcdata[slot][1] = x > 4 ? x - 4 : 0;
	npcdata[slot][2] = y > 4 ? y - 4 : 0;
	npcdata[slot][3] = x + 4;
	npcdata[slot][4] = y + 4;
	npcdata[slot][5] = 1;
	data[160 + x][y] = slot;
	*flags |= 2;
	strcpy(status, "MONSTER PLACED");
}

static void editor_edittrigger(int triggerdata[20][15], int x, int y, char *status)
{
	if(x >= 20 || y >= 15) {
		strcpy(status, "TRIGGER AREA: 20X15");
		return;
	}

	int slot = triggerdata[x][y];
	int values[3] = {(curmap + 1) % 263, 9, 7};
	if(slot >= 0) {
		values[0] = triggers[slot][3];
		values[1] = triggers[slot][1];
		values[2] = triggers[slot][2];
	}
	int selected = 0;
	editor_keypause = ticks + 200;

	for(;;) {
		game_fillrect(videobuffer, 64, 64, 192, 112, 0);
		sys_print(videobuffer, "TELEPORT", 128, 74, 3);
		char line[64];
		sprintf(line, "SOURCE: %02i,%02i", x, y);
		sys_print(videobuffer, line, 88, 88, 0);
		const char *labels[3] = {"DEST MAP", "SPAWN X", "SPAWN Y"};
		for(int i = 0; i < 3; i++) {
			sprintf(line, "%s %03i", labels[i], values[i]);
			sys_print(videobuffer, line, 80, 104 + i * 12, i == selected ? 4 : 0);
		}
		sys_print(videobuffer, "ENTER DEL ESC", 68, 158, 0);
		editor_present();
		sys_pollevent();

		if(event.type == SDL_KEYDOWN && editor_keypause < ticks) {
			int key = event.key.keysym.sym;
			editor_keypause = ticks + 120;
			if(key == SDLK_ESCAPE) return;
			if(key == SDLK_UP || key == SDLK_TAB) selected = (selected + 2) % 3;
			if(key == SDLK_DOWN) selected = (selected + 1) % 3;
			int direction = key == SDLK_LEFT ? -1 : key == SDLK_RIGHT ? 1 : 0;
			if(key == SDLK_PAGEUP) direction = 10;
			if(key == SDLK_PAGEDOWN) direction = -10;
			if(direction) {
				int maximum = selected == 0 ? 262 : selected == 1 ? 19 : 14;
				values[selected] = (values[selected] + direction + maximum + 1) % (maximum + 1);
			}
			if(key == SDLK_DELETE || key == SDLK_BACKSPACE) {
				triggerdata[x][y] = -1;
				strcpy(status, "TELEPORT REMOVED");
				return;
			}
			if(key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_LCTRL || key == SDLK_RCTRL) {
				if(slot < 0) {
					for(int candidate = 1; candidate < 10000 && slot < 0; candidate++) {
						int used = 0;
						for(int j = 0; j < 9; j++) if(triggers[candidate][j] != 0) used = 1;
						if(!used) slot = candidate;
					}
					if(slot < 0) {
						strcpy(status, "NO FREE TRIGGER SLOTS");
						return;
					}
					memset(triggers[slot], 0, sizeof(triggers[slot]));
				}
				triggers[slot][0] = 0;
				triggers[slot][1] = values[1];
				triggers[slot][2] = values[2];
				triggers[slot][3] = values[0];
				triggers[slot][4] = 0;
				triggers[slot][5] = x;
				triggers[slot][6] = y;
				triggerdata[x][y] = slot;
				strcpy(status, "TELEPORT PLACED");
				return;
			}
		}
		sys_tickcalc();
	}
}

static void editor_help(void)
{
	editor_keypause = ticks + 200;

	for(;;) {
		game_fillrect(videobuffer, 40, 16, 240, 208, 0);
		sys_print(videobuffer, "EDITOR HELP", 120, 26, 3);
		sys_print(videobuffer, "ARROWS: MOVE CURSOR", 52, 46, 0);
		sys_print(videobuffer, "G: EDIT TELEPORT", 52, 58, 0);
		sys_print(videobuffer, "T: TILE PALETTE (PGUP/DN: LAYER)", 52, 70, 0);
		sys_print(videobuffer, "E: MONSTER/OBJECT SELECT", 52, 82, 0);
		sys_print(videobuffer, "SHIFT+E: SHOW/HIDE ENTITIES", 52, 94, 0);
		sys_print(videobuffer, "SHIFT: COPY TILE/ENTITY", 52, 106, 0);
		sys_print(videobuffer, "C: SHOW COLLISIONS/TRIGGERS", 52, 118, 0);
		sys_print(videobuffer, "CTRL: TOGGLE COLLISION", 52, 130, 0);
		sys_print(videobuffer, "SPACE: PUT  DEL: CLEAR", 52, 142, 0);
		sys_print(videobuffer, "S: SAVE WITH BACKUP", 52, 154, 0);
		sys_print(videobuffer, "R: RELOAD MAP", 52, 166, 0);
		sys_print(videobuffer, "ESC/F7: EXIT EDITOR", 52, 178, 0);
		sys_print(videobuffer, "F1/ESC/ENTER: CLOSE", 72, 204, 4);
		editor_present();
		sys_pollevent();

		if(event.type == SDL_KEYDOWN && editor_keypause < ticks) {
			int key = event.key.keysym.sym;
			if(key == SDLK_F1 || key == SDLK_ESCAPE || key == SDLK_RETURN) break;
		}
		sys_tickcalc();
	}
	editor_keypause = ticks + 200;
}

static void editor_draw(int data[320][200], int npcdata[MAXNPC][11], int triggerdata[20][15],
	int cursorx, int cursory, int camerax, int cameray, const char *status)
{
	SDL_Rect src, dst;
	char line[128];

	SDL_FillRect(videobuffer, NULL, 0);
	for(int y = 0; y < 15; y++) {
		for(int x = 0; x < 20; x++) {
			int mapx = camerax + x;
			int mapy = cameray + y;
			for(int l = 0; l < 3; l++) {
				int value = data[l * 40 + mapx][mapy];
				if(value <= 0) continue;
				value--;
				src.x = value % 20 * 16;
				src.y = value / 20 * 16;
				src.w = 16;
				src.h = 16;
				dst.x = x * 16;
				dst.y = y * 16;
				dst.w = 16;
				dst.h = 16;
				int surface = data[l * 40 + mapx][mapy + 40];
				if(surface >= 0 && surface < 4) SDL_BlitSurface(tiles[surface], &src, videobuffer, &dst);
			}
			if(editor_showentities && mapx < 20 && mapy < 15) {
				int flags = data[120 + mapx][mapy];
				if(flags & 4) {
					int object = data[200 + mapx][mapy];
					if(object >= 0 && object < 33) editor_drawobject(object, x * 16, y * 16);
				}
				if(flags & 2) {
					int slot = data[160 + mapx][mapy];
					if(slot >= 0 && slot < MAXNPC && npcdata[slot][0] >= 1 && npcdata[slot][0] <= 12)
						editor_drawmonster(npcdata[slot][0], x * 16, y * 16);
				}
			}
			if(editor_collision) {
				if(data[120 + mapx][mapy] % 2 != 0)
					editor_outline(x * 16, y * 16, SDL_MapRGB(videobuffer->format, 255, 32, 32));
				int trigger = mapx < 20 && mapy < 15 ? triggerdata[mapx][mapy] : -1;
				if(trigger >= 0) {
					editor_outline(x * 16, y * 16, SDL_MapRGB(videobuffer->format, 160, 128, 0));
					if(triggers[trigger][0] == 0) {
						sprintf(line, "%i", triggers[trigger][3]);
						sys_print(videobuffer, line, x * 16 + 1, y * 16 + 1, 3);
					}
				}
			}
		}
	}

	editor_outline((cursorx - camerax) * 16, (cursory - cameray) * 16,
		SDL_MapRGB(videobuffer->format, 255, 255, 0));

	game_fillrect(videobuffer, 0, 230, 320, 8, 0);
	if(editor_entitymode)
		sprintf(line, "MAP %03i XY %02i,%02i ENTITY %s %02i F1 HELP", curmap, cursorx, cursory,
			editor_entitykind ? "OBJ" : "NPC", editor_entity);
	else
		sprintf(line, "MAP %03i  XY %02i,%02i  L%i TS%i T%03i  F1 HELP",
			curmap, cursorx, cursory, editor_layer + 1, editor_tileset, editor_tile);
	sys_print(videobuffer, line, 4, 230, 0);
	if(status[0]) sys_print(videobuffer, (char *)status, 4, 200, 1);

	if(editor_tile > 0) {
		src.x = (editor_tile - 1) % 20 * 16;
		src.y = (editor_tile - 1) / 20 * 16;
		src.w = 16;
		src.h = 16;
		dst.x = 296;
		dst.y = 184;
		SDL_BlitSurface(tiles[editor_tileset], &src, videobuffer, &dst);
	}
}

void editor_run(void)
{
	int data[320][200], npcdata[MAXNPC][11], triggerdata[20][15];
	char name[64], status[64] = "";
	editor_keypause = ticks + 250;

	sprintf(name, "mapdb/%04i.map", curmap);
	if(!game_readmap(name, data) || !editor_loadnpc(curmap, npcdata) ||
		!editor_loadtriggers(curmap, triggerdata)) {
		game_eventtext("Cannot open map for editing");
		return;
	}

	game_setpause(1);
	for(;;) {
		editor_draw(data, npcdata, triggerdata, cursorx, cursory, camerax, cameray, status);
		editor_present();
		sys_pollevent();

		if(event.type == SDL_KEYDOWN && editor_keypause < ticks) {
			int key = event.key.keysym.sym;
			editor_keypause = ticks + 120;
			status[0] = '\0';

			if(key == SDLK_ESCAPE || key == SDLK_F7) break;
			if(key == SDLK_F1) editor_help();
			if(key == SDLK_LEFT && cursorx > 0) cursorx--;
			if(key == SDLK_RIGHT && cursorx < 39) cursorx++;
			if(key == SDLK_UP && cursory > 0) cursory--;
			if(key == SDLK_DOWN && cursory < 23) cursory++;
			if(key == SDLK_t) {
				editor_entitymode = 0;
				editor_picktile();
				editor_keypause = ticks + 200;
			}
			if(key == SDLK_e && (event.key.keysym.mod & KMOD_SHIFT)) {
				editor_showentities = !editor_showentities;
			} else if(key == SDLK_e) {
				editor_pickentity();
				editor_keypause = ticks + 200;
			}
			if(key == SDLK_g && cursorx < 20 && cursory < 15) {
				editor_collision = 1;
				editor_edittrigger(triggerdata, cursorx, cursory, status);
				editor_keypause = ticks + 200;
			}
			if(key == SDLK_COMMA && editor_tile > 1) editor_tile--;
			if(key == SDLK_PERIOD) editor_tile++;
			if(key == SDLK_PAGEUP) {
				editor_layer = (editor_layer + 1) % 3;
				editor_entitymode = 0;
			}
			if(key == SDLK_PAGEDOWN) {
				editor_layer = (editor_layer + 2) % 3;
				editor_entitymode = 0;
			}
			if(editor_tile > tiles[editor_tileset]->w / 16 * (tiles[editor_tileset]->h / 16)) editor_tile = 1;
			if(key == SDLK_LSHIFT || key == SDLK_RSHIFT) {
				if(editor_entitymode && cursorx < 20 && cursory < 15) {
					int flags = data[120 + cursorx][cursory];
					if(editor_entitykind && (flags & 4)) {
						editor_entity = data[200 + cursorx][cursory];
						strcpy(status, "OBJECT COPIED");
					} else if(!editor_entitykind && (flags & 2)) {
						int slot = data[160 + cursorx][cursory];
						if(slot >= 0 && slot < MAXNPC) {
							editor_entity = npcdata[slot][0];
							strcpy(status, "MONSTER COPIED");
						}
					}
				} else if(!editor_entitymode) {
					for(int layer = 2; layer >= 0; layer--) {
						if(layer != editor_layer && data[editor_layer * 40 + cursorx][cursory] > 0) continue;
						if(data[layer * 40 + cursorx][cursory] > 0) {
							editor_layer = layer;
							editor_tile = data[layer * 40 + cursorx][cursory];
							editor_tileset = data[layer * 40 + cursorx][cursory + 40];
							strcpy(status, "TILE COPIED");
							break;
						}
					}
				}
			}
			if(key == SDLK_c) editor_collision = !editor_collision;
			if(key == SDLK_LCTRL || key == SDLK_RCTRL) {
				if(data[120 + cursorx][cursory] % 2 != 0) data[120 + cursorx][cursory]--;
				else data[120 + cursorx][cursory]++;
				editor_collision = 1;
				editor_keypause = ticks + 150;
			}
			if(key == SDLK_SPACE) {
				if(editor_entitymode) editor_placeentity(data, npcdata, cursorx, cursory, status);
				else {
					data[editor_layer * 40 + cursorx][cursory] = editor_tile;
					data[editor_layer * 40 + cursorx][cursory + 40] = editor_tileset;
				}
			}
			if(key == SDLK_DELETE || key == SDLK_BACKSPACE) {
				if(cursorx < 20 && cursory < 15 && triggerdata[cursorx][cursory] >= 0) {
					triggerdata[cursorx][cursory] = -1;
					strcpy(status, "TELEPORT REMOVED");
				} else if(editor_entitymode) {
					if(editor_entitykind) data[120 + cursorx][cursory] &= ~4;
					else data[120 + cursorx][cursory] &= ~2;
				} else {
					data[editor_layer * 40 + cursorx][cursory] = 0;
					data[editor_layer * 40 + cursorx][cursory + 40] = 0;
				}
			}
			if(key == SDLK_s && editor_savenpc(curmap, npcdata, status) &&
				editor_savetriggers(curmap, triggerdata, status) && editor_save(curmap, data, status)) game_loadmap(curmap);
			if(key == SDLK_r) {
				sys_LoadTriggers();
				if(game_readmap(name, data) && editor_loadnpc(curmap, npcdata) &&
					editor_loadtriggers(curmap, triggerdata)) strcpy(status, "RELOADED");
				else strcpy(status, "RELOAD FAILED");
			}

			if(cursorx < camerax) camerax = cursorx;
			if(cursorx >= camerax + 20) camerax = cursorx - 19;
			if(cursory < cameray) cameray = cursory;
			if(cursory >= cameray + 13) cameray = cursory - 12;
		}
		sys_tickcalc();
	}

	itemticks = ticks + 250;
	game_setpause(0);
}
