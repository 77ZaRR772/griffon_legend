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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_mixer.h>

#include "griffon.h"
#include "config.h"
#include "state.h"
#include "debug.h"

// fix float warnings
#undef abs
#undef fabsf
#define abs(x) ((x) < 0 ? -(x) : (x))
#define fabsf(x) ((x) < 0 ? -(x) : (x))

// memo 
/*
 in game scripts
   2 - find master key
   3 - find crystal
   4 - find shield - obj 8
   5 - find sword - obj 9
   6 - regular key chest
   7 - blue flask
   8 - garden's master key
   9 - lightning bomb
  10 - blue flask chest
  11 - lightning chest
  12 - armour chest
  13 - citadel master key
  14 - end of game
  15 - get sword3
  16 - shield3
  17 - armour3
  20 key chest 1
  60-lever

 monsters
 1 - baby dragon
 2 - one wing
 3 - boss 1
 4 - black knight
 5 - fire hydra
 6 - red dragon
 7 - priest
 8 - yellow fire dragon
 9 - two wing
10 - dragon2
11 - final boss
12 - bat kitty

 chests
  0 - regular flask
 11 - key chest
 14 - blue flask chest
 15 - lightning chest
 16 - armour chest
 17 - citadel master key
 18 - sword3
 19 - shield3
 20 - armour3

*/

// system
SDL_Surface *video, *videobuffer, *videobuffer2, *videobuffer3;
SDL_Surface *titleimg, *titleimg2, *inventoryimg;
SDL_Surface *logosimg, *theendimg;
SDL_Event event;

SDL_Surface *mapbg, *clipbg, *clipbg2;
unsigned int clipsurround[4][4];
int fullscreen;

Uint8 *keys;
float animspd;
int rampdata[40][24];

int curmap;
SDL_Surface *fontchr[224][5]; // 256 - 32
SDL_Surface *itemimg[21], *windowimg;
SDL_Surface *spellimg;

int itemselon, curitem, itemticks;
char eventtext[128];
int eventtextstart, eventtextend;
float itemyloc;
int selenemyon, curenemy, forcepause;
int roomlock; // set to disable any room jumps while in the room
int scriptflag[100][10], saveslot;	// script, flag

// timer related - move to local later
int ticks, tickspassed, nextticks, game_time;
float fp, fps, fpsr;
int secsingame, secstart;

extern char *story[48];
SDL_Surface *mapimg[4];
extern int invmap[4][7][13];
extern char *story2[27];

SDL_Rect rcSrc, rcDest;

// -----------special case
int dontdrawover;	// used in map24 so that the candles dont draw over the boss, default set to 0

// saveload info
SDL_Surface *saveloadimg;


// post info
float postinfo[21][3];
int nposts;

// cloud info
SDL_Surface *cloudimg;
float clouddeg = 0;
int cloudson;

// spell info
SPELLTYPE spellinfo[MAXSPELL];

// player info
int movingup, movingdown, movingleft, movingright;
PLAYERTYPE player;
int attacking;
PLAYERTYPE playera;
int asecstart;

// tile info
SDL_Surface *tiles[4];
int tileinfo[3][40][24][3]; // maplayer, x, y, tiledata (tile, tilelayer)

extern int elementmap[15][20];


// animation info
SDL_Surface *anims[100];
		// id number 0&1 = players
SDL_Surface *animsa[100];
		// attack anims
float playerattackofs[4][16][3];
		// [dir] [frame] [x,y ofs, completed(0/1)]

float floattext[MAX_FLOATING_TEXTS][4]; // [id] [framesleft, x, y, col]
char *floatstri[MAX_FLOATING_TEXTS];
float  floaticon[MAX_FLOATING_TEXTS][4]; // [id] [framesleft, x, y, ico]

// special for animset2
ANIMSET2TYPE animset2[7], animset9[7];

// object info
float objectframe[256][2];
int lastobj;
		// frame!, curframe
int objectinfo[33][6];
		// nframes,xtiles,ytiles,speed,type,script, update?
int objecttile[33][9][3][3][2];
		// [objnum] [frame] [x] [y] [tile/layer]
int objmap[21][15];

int objmapf[1000][21][15];
		// [mapnum] x, y  set to 1 to make this objmap spot stay at -1

// trigger info
int triggers[10000][9];
		// [map#][index], [var]
		// map#,x,y
int triggerloc[320][240], ntriggers;

// npc info
NPCTYPE npcinfo[MAXNPC];
int lastnpc;

// music info
Mix_Chunk *mgardens, *mgardens2, *mgardens3, *mgardens4, *mboss, *mmenu, *mendofgame;
int menabled=1, musicchannel = -1, menuchannel = -1;
int pgardens, pboss, ptown, pacademy, pcitadel;
int loopseta = 0;

Mix_Chunk *sfx[21];

// room locks
int roomlocks[201], saidlocked, canusekey, locktype, roomtounlock, saidjammed;
// set to 1 for normal key, set to 2 for master, set to 0 if unlocked

// ysort
int ysort[2401], lasty, firsty;

int pmenu;

void sys_tickcalc() {
    int old_ticks = ticks;
    ticks = SDL_GetTicks();
    tickspassed = ticks - old_ticks;

    // fps limit 60 (1000/60 ≈ 16.67 ms)
    if (tickspassed < 16) {
        SDL_Delay(16 - tickspassed);

        // Updating ticks and tickspassed after
        ticks = SDL_GetTicks();
        tickspassed = ticks - old_ticks;
    }

    if(tickspassed <= 0) tickspassed = 1;
    if(tickspassed > 100) tickspassed = 100;

    fpsr = (float)tickspassed / 24.0;

    fp++;
    if(ticks > nextticks) {
        nextticks = ticks + 1000;
        fps = fp;
        fp = 0;
    }
}


#undef main
int main(int argc, char **argv)
{
	mods_parseargs(argc, argv);
	sys_initialize();
    //temp removed.. it works with new cfg var..
	//game_showlogos();
	game_main();

	return 0;
}

// element tile locations
int elementmap[15][20] = {
	{  2, 2, 2, 2,-1,-1,-1, 2, 2, 2, 2, 2, 2,-1,-1,-1,-1,-1,-1,-1 },
	{  2,-1,-1,-1,-1,-1,-1, 2, 2, 2, 2, 2, 2,-1,-1,-1,-1,-1,-1,-1 },
	{  2,-1, 2, 2,-1,-1,-1, 2, 2, 2, 2, 2, 2,-1,-1,-1,-1,-1,-1,-1 },
	{  2,-1, 2,-1, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
	{  2, 2, 2, 2, 2,-1,-1,-1, 2,-1,-1, 2,-1,-1,-1,-1,-1,-1,-1,-1 },
	{ -1,-1,-1,-1, 2,-1, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
	{ -1,-1,-1,-1,-1, 0, 0, 2, 2, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
	{ -1,-1,-1,-1,-1, 2, 2, 2, 2, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
	{ -1,-1,-1,-1,-1, 2, 2, 2, 2, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
	{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
	{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
	{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
	{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
	{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 },
	{ -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 }
};

char *story[48];
static char storytext[48][256];

char *story2[27];
static char story2text[27][256];

// map in inventory menu
int invmap[4][7][13] = {
	// map 0
	{
		{0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,0,43,44,45,46,0,0,0,0},
		{0,0,0,0,0,42,0,0,0,0,0,0,0},
		{0,0,0,0,3,2,0,0,0,0,0,0,0},
		{0,0,0,0,4,5,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0}
	},
	// map 1
	{
		{0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,24,0,0,0,0,0,0,0,0,0},
		{0,0,19,20,21,22,0,0,0,27,0,0,0},
		{0,0,16,17,18,0,0,0,29,30,31,0,0},
		{0,0,12,0,13,14,0,32,33,34,35,36,0},
		{0,8,7,6,9,10,0,37,38,39,40,41,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0}
	},
	// map 2
	{
		{0,0,0,0,0,0,67,0,0,0,0,0,0},
		{0,0,0,0,0,0,66,0,0,0,0,0,0},
		{0,0,0,0,0,63,64,65,0,0,0,0,0},
		{0,0,0,0,58,59,60,61,62,0,0,0,0},
		{0,0,0,0,0,55,56,57,0,0,0,0,0},
		{0,0,0,0,50,51,52,53,54,0,0,0,0},
		{0,0,0,0,0,48,47,49,0,0,0,0,0}
	},

	// map 3
	{
		{0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,82,0,0,0,0,0,0,0,0,0},
		{0,0,0,79,80,81,0,74,72,0,0,0,0},
		{0,0,0,78,0,0,0,73,70,69,68,0,0},
		{0,0,77,76,75,0,0,0,71,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0}
	}
};

// HELPER MACRO ---------------------------------------------------------------
#define PRINT(A,B) 			\
do {					\
	char line[256];			\
	sprintf(line, A "\n", B);	\
	fputs(line, fp);		\
} while(0)

#define INPUT(A, B)			\
do {					\
	char line[256];			\
	fgets(line, sizeof(line), fp);	\
	sscanf(line, A, B);		\
} while(0)

// CODE GOES HERE -------------------------------------------------------------

#ifdef OPENDINGUX
  #define SDL_BLITVIDEO(X, Y, C, F) SDL_BlitSurface((X), (Y), (C), (F))
#else
  #define SDL_BLITVIDEO(X, Y, C, F) sdl_blitscale((X), (Y), (C), NULL)
#endif

static SDL_Surface *game_setvideomode(int width, int height, int bpp, int flags)
{
	if(config.stretch && (flags & SDL_FULLSCREEN)) {
		const SDL_VideoInfo *info = SDL_GetVideoInfo();
		if(info && info->current_w > 0 && info->current_h > 0) {
			width = info->current_w / 2;
			height = info->current_h / 2;
		}
	}
	return SDL_SetVideoMode(width, height, bpp, flags);
}

void game_fillrect(SDL_Surface *surface, int x, int y, int w, int h, int color)
{
	SDL_Rect src;

	src.x = x;
	src.y = y;
	src.w = w;
	src.h = h;

	SDL_FillRect(surface, &src, color);
}

void game_playsound(int sound)
{
	if(menabled != 1 || config.effects != 1) return;
	int channel = Mix_PlayChannel(-1, sfx[sound], 0);
	if(channel >= 0) Mix_Volume(channel, config.effectsvol);
}

// copypaste from hRnd_CRT()
static float RND()
{
	/* return between 0 and 1 (but never 1) */
	return (float)rand() * (1.0 / ((float)RAND_MAX + 1.0));
}

int windowfocused = 1;

static SDL_Surface *sys_loadimage(const char *name)
{
	SDL_Surface *image = IMG_Load(mods_path(name));
	if(!image) {
		fprintf(stderr, "Failed to load image %s: %s\n", name, IMG_GetError());
		exit(1);
	}
	return image;
}

static Mix_Chunk *sys_loadchunk(const char *name)
{
	Mix_Chunk *chunk = Mix_LoadWAV(mods_path(name));
	if(!chunk) {
		fprintf(stderr, "Failed to load audio %s: %s\n", name, Mix_GetError());
		exit(1);
	}
	return chunk;
}

static Mix_Chunk *sys_loadmodchunk(const char *name)
{
	const char *path = mods_localpath(name);
	return path ? Mix_LoadWAV(path) : NULL;
}

void game_addFloatIcon(int ico, float xloc, float yloc)
{
	for(int i = 0; i < MAX_FLOATING_TEXTS; i++) {
		if(floaticon[i][0] == 0) {
			floaticon[i][0] = 32;
			floaticon[i][1] = xloc;
			floaticon[i][2] = yloc;
			floaticon[i][3] = ico;
			return;
		}
	}
}

void game_addFloatText(char *stri, float xloc, float yloc, int col)
{
	for(int i = 0; i < MAX_FLOATING_TEXTS; i++) {
		if(floattext[i][0] == 0) {
			floattext[i][0] = 32;
			floattext[i][1] = xloc;
			floattext[i][2] = yloc;
			floattext[i][3] = col;
			strcpy(floatstri[i], stri);
			return;
		}
	}
}

void game_checkhit()
{
	float npx, npy;
	float damage;

	if(attacking == 1) {
		for(int i = 1; i <= lastnpc; i++) {
			if(npcinfo[i].hp > 0 && npcinfo[i].pause < game_time && (int)(RND() * 2) == 0) {
				npx = npcinfo[i].x;
				npy = npcinfo[i].y;

				float xdif = player.px - npx;
				float ydif = player.py - npy;

				float ps = player.sword;
				if(ps > 1) ps = ps * 0.75;
					damage = (float)player.sworddamage * (1.0 + RND() * 1.0) * player.attackstrength / 100.0 * ps;
					if(player.attackstrength == 100) damage = damage * 1.5;

					int hit = 0;
					if(player.walkdir == 0) {
						if(abs(xdif) <= 8 && ydif >= 0 && ydif < 8) hit = 1;
					} else if(player.walkdir == 1) {
						if(abs(xdif) <= 8 && ydif <= 0 && ydif > -8) hit = 1;
					} else if(player.walkdir == 2) {
						if(abs(ydif) <= 8 && xdif >= -8 && xdif < 8) hit = 1;
					} else if(player.walkdir == 3) {
						if(abs(ydif) <= 8 && xdif <= 8 && xdif > -8) hit = 1;
					}

					if(hit == 1) {
						if(menabled == 1 && config.effects == 1) {
							static int last_hit_sound_tick = 0;
							if(ticks - last_hit_sound_tick > 150) {
								int snd = Mix_PlayChannel(-1, sfx[sndswordhit], 0);
								if(snd >= 0) {
									Mix_Volume(snd, config.effectsvol);
								}
								last_hit_sound_tick = ticks;
							}
						}
						game_damagenpc(i, damage, 0);
					}
			}
		}
	}
}

void game_setpause(int paused)
{
	forcepause = paused;
}

static void game_quickspell(int item)
{
	int spell = item - 1;
	if(controlled_npc > 0 || itemselon || itemticks >= ticks) return;
	if(item > 0 && (spell < 0 || !player.foundspell[item] || player.spellcharge[item] < 100)) return;

	if(item == 0) {
		if(player.spellcharge[0] < 100) return;
		game_castspell(5, player.px, player.py, player.px, player.py, 0);
		player.spellcharge[0] = 0;
		itemticks = ticks + 175;
		return;
	}

	for(int i = 0; i <= lastnpc; i++) {
		if(npcinfo[i].hp > 0) {
			game_castspell(spell, player.px, player.py, npcinfo[i].x, npcinfo[i].y, 0);
			player.spellcharge[item] = 0;
			player.spellstrength = 0;
			itemticks = ticks + 175;
			return;
		}
	}
	if(nposts > 0) {
		game_castspell(spell, player.px, player.py, postinfo[0][0], postinfo[0][1], 0);
		player.spellcharge[item] = 0;
		player.spellstrength = 0;
		itemticks = ticks + 175;
	}
}

void game_checkinputs()
{
	int ntickdelay;

	ntickdelay = 175;

	sys_pollevent();
	if(event.type == SDL_KEYUP) {
		int keycode = event.key.keysym.sym;
		if(keycode == config.key_dbg) keycode = SDLK_F12;
		debug_keyup(keycode);
	}
	keys = SDL_GetKeyState(NULL);

	nposts = 0;

	for(int i = 0; i <= 20; i++) {
		postinfo[i][0] = 0;
		postinfo[i][1] = 0;
	}

	for(int x = 0; x <= 19; x++) {
		for(int y = 0; y <= 14; y++) {
			int o = objmap[x][y];
			if(o >= 0 && objectinfo[o][4] == 3) {
				postinfo[nposts][0] = x * 16;
				postinfo[nposts][1] = y * 16;
				nposts = nposts + 1;
			}
		}
	}

	int lightningbomb = spells_lightningactive();
	if(attacking == 1 || (forcepause == 1 && itemselon == 0 && !lightningbomb)) return;

	if(event.type == SDL_KEYDOWN && !lightningbomb) {
		int keycode = event.key.keysym.sym;
		if(keycode == config.key_menu) keycode = SDLK_ESCAPE;
		if(keycode == config.key_attack) keycode = SDLK_LCTRL;
		if(keycode == config.key_item) keycode = SDLK_LALT;
		if(keycode == config.key_dbg) keycode = SDLK_F12;
		if(keycode == config.key_cheat_god) keycode = SDLK_F11;
		if(keycode == config.key_cheat_noclip) keycode = SDLK_F10;

		if(debug_handlekey(keycode)) return;
		if(keycode == config.key_spell1) game_quickspell(0);
		if(keycode == config.key_spell2) game_quickspell(1);
		if(keycode == config.key_spell3) game_quickspell(2);
		if(keycode == config.key_spell4) game_quickspell(3);
		if(keycode == config.key_spell5) game_quickspell(4);

		if(selenemyon && monster_target && controlled_npc == 0 && itemticks < ticks) {
			if(keycode == config.key_left || keycode == config.key_up) debug_npctarget(-1);
			if(keycode == config.key_right || keycode == config.key_down) debug_npctarget(1);
			itemticks = ticks + 250;
		}

		switch(keycode) {
		case SDLK_ESCAPE:
			if(selenemyon && monster_target && controlled_npc == 0) {
				selenemyon = 0;
				itemticks = ticks + 200;
			} else if(itemticks < ticks) {
				int waspaused = forcepause;
				game_setpause(1);
				game_title(1);
				if(!waspaused) game_setpause(0);
			}
			break;
		case SDLK_RETURN:
			if(keys[SDLK_LALT] || keys[SDLK_RALT]) {
				if(fullscreen & SDL_FULLSCREEN) {
					fullscreen = config.hwaccel | config.hwsurface;
				} else {
					fullscreen = SDL_FULLSCREEN | config.hwaccel | config.hwsurface;
				}

				config.fullscreen = fullscreen & SDL_FULLSCREEN;

				video = game_setvideomode(config.scr_width, config.scr_height, config.scr_bpp, fullscreen);
				SDL_UpdateRect(video, 0, 0, config.scr_width, config.scr_height);
			}
			break;

		case SDLK_LCTRL:
			if(selenemyon && monster_target && controlled_npc == 0) {
				controlled_npc = curenemy;
				selenemyon = 0;
				monster_target = 0;
				game_addFloatText("CONTROLLED", npcinfo[curenemy].x, npcinfo[curenemy].y, 3);
				itemticks = ticks + 250;
			} else if(controlled_npc > 0 && itemticks < ticks) debug_controlledattack(0);
			else if(itemselon == 0 && itemticks < ticks) game_attack();

			if(itemselon == 1 && itemticks < ticks) {
				if(curitem == 0 && player.inventory[INV_FLASK] > 0) {
					itemticks = ticks + ntickdelay;

					if(player.hp >= player.maxhp) {
						game_playsound(sndbeep);
						break;
					}

					int heal = 50;
					int maxh = player.maxhp - player.hp;

					if(heal > maxh) heal = maxh;

					player.hp = player.hp + heal;

					char text[256];
					sprintf(text, "+%i", heal);
					game_addFloatText(text, player.px + 16 - 4 * strlen(text), player.py + 16, 5);

					player.inventory[INV_FLASK]--;

					game_playsound(sndpowerup);

					itemselon = 0;
					game_setpause(0);
				}

				if(curitem == 1 && player.inventory[INV_DOUBLEFLASK] > 0) {
					itemticks = ticks + ntickdelay;

					if(player.hp >= player.maxhp) {
						game_playsound(sndbeep);
						break;
					}

					int heal = 200;
					int maxh = player.maxhp - player.hp;

					if(heal > maxh) heal = maxh;

					player.hp = player.hp + heal;

					char text[256];
					sprintf(text, "+%i", heal);
					game_addFloatText(text, player.px + 16 - 4 * strlen(text), player.py + 16, 5);

					player.inventory[INV_DOUBLEFLASK]--;

					game_playsound(sndpowerup);

					itemselon = 0;
					game_setpause(0);

				}

				if(curitem == 2 && player.inventory[INV_SHOCK] > 0) {
					game_castspell(8, player.px, player.py, npcinfo[curenemy].x, npcinfo[curenemy].y, 0);

					game_setpause(1);

					player.inventory[INV_SHOCK]--;

					itemticks = ticks + ntickdelay;
					selenemyon = 0;
					itemselon = 0;

				}

				if(curitem == 3 && player.inventory[INV_NORMALKEY] > 0 && canusekey == 1 && locktype == 1) {
					roomlocks[roomtounlock] = 0;
					game_eventtext("UnLocked!");

					player.inventory[INV_NORMALKEY]--;

					itemticks = ticks + ntickdelay;
					selenemyon = 0;
					itemselon = 0;
					return;
				}

				if(curitem == 4 && player.inventory[INV_MASTERKEY] > 0 && canusekey == 1 && locktype == 2) {
					roomlocks[roomtounlock] = 0;
					game_eventtext("UnLocked!");

					player.inventory[INV_MASTERKEY]--;

					itemticks = ticks + ntickdelay;
					selenemyon = 0;
					itemselon = 0;
					return;
				}

				if(curitem == 5 && player.spellcharge[0] == 100) {
					game_castspell(5, player.px, player.py, npcinfo[curenemy].x, npcinfo[curenemy].y, 0);

					player.spellcharge[0] = 0;

					game_setpause(0);

					itemticks = ticks + ntickdelay;
					selenemyon = 0;
					itemselon = 0;
				}

				if(curitem > 5 && selenemyon == 1) {
					if(curenemy <= lastnpc) {
						game_castspell(curitem - 6, player.px, player.py, npcinfo[curenemy].x, npcinfo[curenemy].y, 0);
					} else {
						int pst = curenemy - lastnpc - 1;
						game_castspell(curitem - 6, player.px, player.py, postinfo[pst][0], postinfo[pst][1], 0);
					}

					player.spellcharge[curitem - 5] = 0;

					player.spellstrength = 0;

					itemticks = ticks + ntickdelay;
					selenemyon = 0;
					itemselon = 0;
					game_setpause(0);
				}

				if(curitem > 5 && selenemyon == 0 && itemselon == 1) {
					if(player.spellcharge[curitem - 5] == 100) {
						itemticks = ticks + ntickdelay;

						selenemyon = 1;

						int i = 0;
						do {
							if(npcinfo[i].hp > 0) {
								curenemy = i;
								goto __exit_do;
							}
							i = i + 1;
							if(i == lastnpc + 1) {
								selenemyon = 0;
								goto __exit_do;
							}
						} while(1);
					__exit_do:

						if(nposts > 0 && selenemyon == 0) {
							selenemyon = 1;
							curenemy = lastnpc + 1;
						}
					}

				}
			}
			break;

		case SDLK_LALT:
			if(controlled_npc > 0 && itemticks < ticks) {
				debug_controlledattack(1);
				itemticks = ticks + ntickdelay;
				break;
			}
			if(itemticks < ticks) {
				selenemyon = 0;
				monster_target = 0;
				if(itemselon == 1) {
					itemselon = 0;
					itemticks = ticks + 220;
					game_setpause(0);
				} else {
					itemselon = 1;
					itemticks = ticks + 220;
					game_setpause(1);
					player.itemselshade = 0;
				}
			}
			break;

		default:
			;
		}
	}

	if(itemselon == 0) {
		movingup = 0;
		movingdown = 0;
		movingleft = 0;
		movingright = 0;
		if(keys[config.key_up]) movingup = 1;
		if(keys[config.key_down]) movingdown = 1;
		if(keys[config.key_left]) movingleft = 1;
		if(keys[config.key_right]) movingright = 1;
	} else {
		movingup = 0;
		movingdown = 0;
		movingleft = 0;
		movingright = 0;

		if(selenemyon == 1) {
			if(itemticks < ticks) {
				if(keys[config.key_left]) {
					int origin = curenemy;
					do {
						curenemy = curenemy - 1;
						if(curenemy < 1) curenemy = lastnpc + nposts;
						if(curenemy == origin) break;
						if(curenemy <= lastnpc && npcinfo[curenemy].hp > 0) break;
						if(curenemy > lastnpc) break;
					} while(1);
					itemticks = ticks + ntickdelay;
				}
				if(keys[config.key_right]) {
					int origin = curenemy;
					do {
						curenemy = curenemy + 1;
						if(curenemy > lastnpc + nposts) curenemy = 1;
						if(curenemy == origin) break;
						if(curenemy <= lastnpc && npcinfo[curenemy].hp > 0) break;
						if(curenemy > lastnpc) break;
					} while(1);
					itemticks = ticks + ntickdelay;
				}


				if(curenemy > lastnpc + nposts) curenemy = 1;
				if(curenemy < 1) curenemy = lastnpc + nposts;
			}
		} else {
			if(itemticks < ticks) {
				if(keys[config.key_up]) {
					curitem = curitem - 1;
					itemticks = ticks + ntickdelay;
					if(curitem == 4) curitem = 9;
					if(curitem == -1) curitem = 4;
				}
				if(keys[config.key_down]) {
					curitem = curitem + 1;
					itemticks = ticks + ntickdelay;
					if(curitem == 5) curitem = 0;
					if(curitem == 10) curitem = 5;
				}
				if(keys[config.key_left]) {
					curitem = curitem - 5;
					itemticks = ticks + ntickdelay;
				}
				if(keys[config.key_right]) {
					curitem = curitem + 5;
					itemticks = ticks + ntickdelay;
				}

				if(curitem > 9) curitem = curitem - 10;
				if(curitem < 0) curitem = curitem + 10;
			}
		}
	}
}

void game_checktrigger()
{
	int npx, npy, lx, ly;

	npx = player.px + 12;
	npy = player.py + 20;

	lx = (int)npx / 16;
	ly = (int)npy / 16;

	canusekey = 0;

	if(triggerloc[lx][ly] > -1) game_processtrigger(triggerloc[lx][ly]);
}

#ifdef OPENDINGUX
	#define MINCURSEL 7
	#define MAXCURSEL 14
	#define SY 22
#else
	#define MINCURSEL 0
	#define MAXCURSEL 14
	#define SY (38 + (240 - 38) / 2 - 88)
#endif

void game_configmenu()
{
	SDL_Surface *configwindow;
	SDL_Rect rc;
	int cursel, curselt, ofullscreen;
	int tickwait, keypause, ticks1;

	cursel = MINCURSEL;

	tickwait = 100;
	keypause = ticks + tickwait;

	configwindow = sys_loadimage("art/configwindow.bmp");
	SDL_SetColorKey(configwindow, SDL_SRCCOLORKEY, SDL_MapRGB(configwindow->format, 255, 0, 255));
	SDL_SetAlpha(configwindow, SDL_SRCALPHA, 160);

	ticks1 = ticks;
	do {
		SDL_FillRect(videobuffer, NULL, 0);

		rcDest.x = 256 + 256 * cos(3.141592 / 180 * clouddeg * 40);
		rcDest.y = 192 + 192 * sin(3.141592 / 180 * clouddeg * 40);
		rcDest.w = 320;
		rcDest.h = 240;

		SDL_SetAlpha(cloudimg, SDL_SRCALPHA, 128);
		SDL_BlitSurface(cloudimg, &rcDest, videobuffer, NULL);
		SDL_SetAlpha(cloudimg, SDL_SRCALPHA, 64);

		rcDest.x = 256;
		rcDest.y = 192;
		rcDest.w = 320;
		rcDest.h = 240;

		SDL_SetAlpha(cloudimg, SDL_SRCALPHA, 128);
		SDL_BlitSurface(cloudimg, &rcDest, videobuffer, NULL);
		SDL_SetAlpha(cloudimg, SDL_SRCALPHA, 64);

		SDL_BlitSurface(configwindow, NULL, videobuffer, NULL);

		int sy = SY;

		for(int i = 0; i <= 22; i++) {
			static char *vr[23] = {
#ifdef OPENDINGUX
				"", "",
				"", "", "", "",
				"", "", "",
#else
				"Resolution:", "",
				"Bit Depth:", "", "", "",
				"Start Fullscreen:", "", "",
				"Stretch Screen:", "", "",
#endif
				"Music:", "", "",
				"Sound Effects:", "", "",
				"Music Volume:", "",
				"Effects Volume:", "", ""
			};
			static char *vl[23] = {
#ifdef OPENDINGUX
				"", "",
				"", "", "", "",
				"", "", "",
#else
				"320x240", "640x480",
				"16", "24", "32", "",
				"Yes", "No", "",
				"Yes", "No", "",
#endif
				"On", "Off", "",
				"On", "Off", "",
				"[----------]", "",
				"[----------]", "", "Exit"
			};

			static char line[24];

			if(i == 18 || i == 20) {
				int vol = (i == 18 ? config.musicvol : config.effectsvol) * 9 / 255;
				if(vol < 0) vol = 0;
				if(vol > 9) vol = 9;

				strcpy(line, "[----------]");
				line[vol+1] = 'X';
				vl[i] = line;
			}

			int cl = 3;
			if(i == 0 && config.scr_width == 320) cl = 0;
			if(i == 1 && config.scr_width == 640) cl = 0;
			if(i == 2 && config.scr_bpp == 16) cl = 0;
			if(i == 3 && config.scr_bpp == 24) cl = 0;
			if(i == 4 && config.scr_bpp == 32) cl = 0;
			if(i == 6 && config.fullscreen != 0) cl = 0;
			if(i == 7 && config.fullscreen == 0) cl = 0;
			if(i == 9 && config.stretch != 0) cl = 0;
			if(i == 10 && config.stretch == 0) cl = 0;
			if(i == 12 && config.music == 1) cl = 0;
			if(i == 13 && config.music == 0) cl = 0;
			if(i == 15 && config.effects == 1) cl = 0;
			if(i == 16 && config.effects == 0) cl = 0;

			if(i > 20) cl = 0;

			sys_print(videobuffer, vr[i], 156 - 8 * strlen(vr[i]), sy + i * 8, 0);
			sys_print(videobuffer, vl[i], 164, sy + i * 8, cl);
		}

		curselt = cursel;
		if(cursel > 4) curselt = curselt + 1;
		if(cursel > 6) curselt = curselt + 1;
		if(cursel > 8) curselt = curselt + 1;
		if(cursel > 10) curselt = curselt + 1;
		if(cursel > 12) curselt = curselt + 1;
		if(cursel > 13) curselt = curselt + 1;
		if(cursel > 14) curselt = curselt + 1;

		rc.x = 148 + 3 * cos(3.14159 * 2 * itemyloc / 16.0);
		rc.y = sy + 8 * curselt - 4;

		SDL_BlitSurface(itemimg[15], NULL, videobuffer, &rc);

		float yy = 255.0;
		if(ticks < ticks1 + 1000) {
			yy = 255.0 * ((float)(ticks - ticks1) / 1000.0);
			if(yy < 0.0) yy = 0.0;
			if(yy > 255.0) yy = 255.0;
		}

		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, (int)yy);
		SDL_BLITVIDEO(videobuffer, NULL, video, NULL);
		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, 255);

		SDL_Flip(video);
		SDL_PumpEvents();

		sys_tickcalc();

		itemyloc += 0.75 * fpsr;
		while(itemyloc >= 16) itemyloc -= 16;

		if(keypause < ticks) {
			sys_pollevent();
			keys = SDL_GetKeyState(NULL);

			if(event.type == SDL_KEYDOWN) {
				keypause = ticks + tickwait;

				if(keys[config.key_menu]) break;

				if(keys[config.key_left]) {
					if(cursel == 13) {
						config.musicvol = config.musicvol - 25;
						if(config.musicvol < 0) config.musicvol = 0;

						Mix_Volume(musicchannel, config.musicvol);
						Mix_Volume(menuchannel, config.musicvol);
					} else if(cursel == 14) {
						config.effectsvol = config.effectsvol - 25;
						if(config.effectsvol < 0) config.effectsvol = 0;

						Mix_Volume(-1, config.effectsvol);
						Mix_Volume(musicchannel, config.musicvol);
						Mix_Volume(menuchannel, config.musicvol);

						game_playsound(snddoor);
					}
				}
				if(keys[config.key_right]) {
					if(cursel == 13) {
						config.musicvol = config.musicvol + 25;
						if(config.musicvol > 255) config.musicvol = 255;

						Mix_Volume(musicchannel, config.musicvol);
						Mix_Volume(menuchannel, config.musicvol);
					} else if(cursel == 14) {
						config.effectsvol = config.effectsvol + 25;
						if(config.effectsvol > 255) config.effectsvol = 255;

						Mix_Volume(-1, config.effectsvol);
						Mix_Volume(musicchannel, config.musicvol);
						Mix_Volume(menuchannel, config.musicvol);

						game_playsound(snddoor);
					}
				}

				if(keys[config.key_up]) {
					cursel--;
					if(cursel < MINCURSEL) cursel = 15;
				}
				if(keys[config.key_down]) {
					cursel++;
					if(cursel > 15) cursel = MINCURSEL;
				}

				if(keys[config.key_attack] || keys[SDLK_RETURN]) {
					if(cursel == 0) {
						fullscreen = config.fullscreen | config.hwaccel | config.hwsurface;

						video = game_setvideomode(320, 240, config.scr_bpp, fullscreen);
						if(video == 0) {
							video = game_setvideomode(config.scr_width, config.scr_height, config.scr_bpp, fullscreen);
						} else {
							config.scr_width = 320;
							config.scr_height = 240;
						}

						SDL_UpdateRect(video, 0, 0, config.scr_width, config.scr_height);
					}
					if(cursel == 1) {
						fullscreen = config.fullscreen | config.hwaccel | config.hwsurface;

						video = game_setvideomode(640, 480, config.scr_bpp, fullscreen);
						if(video == 0) {
							video = game_setvideomode(config.scr_width, config.scr_height, config.scr_bpp, fullscreen);
						} else {
							config.scr_width = 640;
							config.scr_height = 480;
						}

						SDL_UpdateRect(video, 0, 0, config.scr_width, config.scr_height);
					}
					if(cursel == 2 || cursel == 3 || cursel == 4) {
						fullscreen = config.fullscreen | config.hwaccel | config.hwsurface;

						int b = 16;
						if(cursel == 3) b = 24;
						if(cursel == 4) b = 32;
						video = game_setvideomode(config.scr_width, config.scr_height, b, fullscreen);
						if(video == 0) {
							video = game_setvideomode(config.scr_width, config.scr_height, config.scr_bpp, fullscreen);
						} else {
							config.scr_bpp = b;
						}

						SDL_UpdateRect(video, 0, 0, config.scr_width, config.scr_height);
					}
					if(cursel == 5) {
						ofullscreen = config.fullscreen | config.hwaccel | config.hwsurface;
						fullscreen = SDL_FULLSCREEN | config.hwaccel | config.hwsurface;

						video = game_setvideomode(config.scr_width, config.scr_height, config.scr_bpp, fullscreen);
						if(video == 0) {
							video = game_setvideomode(config.scr_width, config.scr_height, config.scr_bpp, ofullscreen);
						} else {
							config.fullscreen = SDL_FULLSCREEN;
						}

						SDL_UpdateRect(video, 0, 0, config.scr_width, config.scr_height);
					}
					if(cursel == 6) {
						ofullscreen = config.fullscreen | config.hwaccel | config.hwsurface;
						fullscreen = 0 | config.hwaccel | config.hwsurface;

						video = game_setvideomode(config.scr_width, config.scr_height, config.scr_bpp, fullscreen);
						if(video == 0) {
							video = game_setvideomode(config.scr_width, config.scr_height, config.scr_bpp, ofullscreen);
						} else {
							config.fullscreen = 0;
						}

						SDL_UpdateRect(video, 0, 0, config.scr_width, config.scr_height);
					}
					if(cursel == 7 || cursel == 8) {
						config.stretch = cursel == 7;
						video = game_setvideomode(config.scr_width, config.scr_height, config.scr_bpp, fullscreen);
					}
					if(cursel == 9 && config.music == 0) {
						config.music = 1;
						if(menabled == 1) {
							menuchannel = Mix_PlayChannel(-1, mmenu, -1);
							Mix_Volume(menuchannel, config.musicvol);
						}
					}
					if(cursel == 10 && config.music == 1) {
						config.music = 0;
						Mix_HaltChannel(musicchannel);
						Mix_HaltChannel(menuchannel);
					}
					if(cursel == 11 && config.effects == 0) {
						config.effects = 1;
						if(menabled == 1) {
							int snd = Mix_PlayChannel(-1, sfx[snddoor], 0);
							Mix_Volume(snd, config.effectsvol);
						}
					}

					if(cursel == 12 && config.effects == 1) config.effects = 0;

					if(cursel == 15) {
						config_save(&config);
						keys[SDLK_SPACE] = keys[SDLK_RETURN] = 0;
						break;
					}
				}
			}
		}

		clouddeg += 0.01 * fpsr;
		while(clouddeg >= 360) clouddeg = clouddeg - 360;

		} while(1);

	SDL_FreeSurface(configwindow);
	itemticks = ticks + 210;

	SDL_SetAlpha(cloudimg, SDL_SRCALPHA, 64);
}

void game_damagenpc(int npcnum, int damage, int spell)
{
	char line[256];
	int fcol;

	if(damage == 0) {
		//prevent spam on a lot enemies
		if (ticks - npcinfo[npcnum].lastmisstick < 3000) return;
		npcinfo[npcnum].lastmisstick = ticks;

		strcpy(line, "miss!");
		fcol = 2;
	} else {
		int ratio = 0;
		int heal = (damage < 0) ? 1 : 0;
		damage = abs(damage);

		if(heal == 0) {
			if(damage > npcinfo[npcnum].hp) {
				ratio = (damage - npcinfo[npcnum].hp) * 100 / damage;
				damage = npcinfo[npcnum].hp;
			}

			npcinfo[npcnum].hp -= damage;
			if(npcinfo[npcnum].hp < 0) npcinfo[npcnum].hp = 0;
			if(controlled_npc > 0 && npcnum != controlled_npc) npcinfo[npcnum].aggro = 1;

			sprintf(line, "-%i", damage);
			fcol = 1;
		} else {
			npcinfo[npcnum].hp += damage;
			if(npcinfo[npcnum].hp > npcinfo[npcnum].maxhp) npcinfo[npcnum].hp = npcinfo[npcnum].maxhp;

			sprintf(line, "+%i", damage);
			fcol = 5;
		}

		npcinfo[npcnum].pause = game_time + 900;

		if(spell == 0) player.attackstrength = ratio;
	}

	game_addFloatText(line, npcinfo[npcnum].x + 12 - 4 * strlen(line), npcinfo[npcnum].y + 16, fcol);

	if(npcinfo[npcnum].spriteset == 12) {
		game_castspell(9, npcinfo[npcnum].x, npcinfo[npcnum].y, player.px, player.py, npcnum);
	}

	// if enemy is killed
	if(npcinfo[npcnum].hp != 0) return;

	if(controlled_npc == 0 && !npc_infight) player.exp += npcinfo[npcnum].maxhp;

	// Random drops based on spriteset
	if(npcinfo[npcnum].spriteset == 1 || npcinfo[npcnum].spriteset == 7 || npcinfo[npcnum].spriteset == 6) {
		items_drop(npcnum, 4, 3);
	}
	if(npcinfo[npcnum].spriteset == 2 || npcinfo[npcnum].spriteset == 9 || npcinfo[npcnum].spriteset == 4 || npcinfo[npcnum].spriteset == 5) {
		items_drop(npcnum, 12, 1);
	}
	if(npcinfo[npcnum].spriteset == 9 || npcinfo[npcnum].spriteset == 10 || npcinfo[npcnum].spriteset == 5) {
		items_drop(npcnum, 13, 2);
	}

	int script = npcinfo[npcnum].script;

	if(script == 2) {
		items_spawnchest(9, 7, 5, 2);
	} else if(script == 3) {
		items_spawnchest(9, 7, 6, 3);
	} else if(script == 4 && scriptflag[4][0] == 0) {
		if(items_allenemiesdead()) {
			triggerloc[9][7] = 5004;

			int curtile = 40;
			int curtilel = 0;
			int curtilex = curtile % 20;
			int curtiley = (curtile - curtilex) / 20;

			tileinfo[0][9][7][0] = curtile + 1;
			tileinfo[0][9][7][1] = 0;

			rcSrc.x = curtilex * 16;
			rcSrc.y = curtiley * 16;
			rcSrc.w = 16;
			rcSrc.h = 16;

			rcDest.x = 9 * 16;
			rcDest.y = 7 * 16;
			rcDest.w = 16;
			rcDest.h = 16;

			SDL_BlitSurface(tiles[curtilel], &rcSrc, mapbg, &rcDest);
			scriptflag[4][0] = 1; // in original flag didn't apply
		}
	} else if(script == 5) {
		items_spawnchest(9, 6, 9, 5);
	} else if(script == 8 && scriptflag[6][0] == 0) { // in original its indeed scriptflag[6][0], not 8 as should, maybe bug
		items_spawnchest(13, 7, 5, 8);
	} else if(script >= 20 && script <= 23 && scriptflag[script][0] < 2) {
		items_spawnchest(9, 7, 11, script);
	} else if(script == 9 && curmap == 41 && scriptflag[9][1] == 0) {
		if(items_allenemiesdead()) {
			objmap[9][7] = 13;

			int lx = (int)(player.px + 12) / 16;
			int ly = (int)(player.py + 20) / 16;

			if(lx == 9 && ly == 7) player.py = player.py + 16;
			scriptflag[9][1] = 1; // in original flag didn't apply
		}
	} else if(script == 12) {
		items_spawnchest(8, 7, 16, 12);
	} else if(script == 13 && scriptflag[13][0] == 0) {
		items_spawnchest(11, 10, 5, 13);
	} else if(script == 15 && scriptflag[15][0] == 0) {
		if(items_allenemiesdead()) {
			items_spawnchest(6, 8, 18, 15);
			items_spawnchest(9, 8, 19, 16);
			items_spawnchest(12, 8, 20, 17);
		}
	} else if(script == 14) {
		game_endofgame();
	}
}

void game_damageplayer(int damage)
{
	char line[256];

	if(ai_target > 0) {
		game_damagenpc(ai_target, damage, 1);
		return;
	}
	if(cheat_god) return;

	//improvement. Now shield and armour gives defense bonus. No more intense hard grinding.
	damage = (damage - (damage * (player.shield + player.armour) / 20) > 1) ? damage - (damage * (player.shield + player.armour) / 20) : 1;

	player.hp -= damage;
	if(player.hp < 0) player.hp = 0;

	sprintf(line, "-%i", damage);
	if(damage == 0) strcpy(line, "miss!");

	game_addFloatText(line, player.px + 12 - 4 * strlen(line), player.py + 16, 4);

	player.pause = game_time + 1000;

	//printf("Player shield/armor: %i/%i\n", player.shield, player.armour);
	//printf("Player total_def: %i\n", total_def);
	//sprintf(line, "Player shield/armor: %i/%i\n", player.shield, player.armour);
}

void game_endofgame()
{
	float xofs = 0;
	int ticks1;

	//ticks = SDL_GetTicks();

	float spd = 0.2;

	if(menabled == 1 && config.music == 1) {
		Mix_HaltChannel(-1);
		musicchannel = Mix_PlayChannel(-1, mendofgame, -1);
		Mix_Volume(musicchannel, 0);
	}

	ticks1 = ticks;
	int ya = 0;

	SDL_FillRect(videobuffer2, NULL, 0);
	SDL_FillRect(videobuffer3, NULL, 0);
	SDL_BlitSurface(videobuffer, NULL, videobuffer2, NULL);

	float ld = 0;
	int ldstop = 0;

	do {
		ld = ld + 4 * fpsr;
		if(ld > config.musicvol) ld = config.musicvol;
		if(menabled == 1 && ldstop == 0) {
			Mix_Volume(musicchannel, (int)ld);
			if((int)ld == config.musicvol) ldstop = 1;
		}

		ya = 0;
		if(ticks < ticks1 + 1500) {
			ya = (255 * (ticks - ticks1)) / 1500;
			if(ya < 0) ya = 0;
			if(ya > 255) ya = 255;
		} else {
			break;
		}

		SDL_FillRect(videobuffer, NULL, 0);

		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, ya);
		SDL_BlitSurface(videobuffer2, NULL, videobuffer3, NULL);
		SDL_BlitSurface(videobuffer, NULL, videobuffer3, NULL);
		SDL_BLITVIDEO(videobuffer3, NULL, video, NULL);
		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, 255);

		SDL_Flip(video);
		SDL_PumpEvents();

		sys_tickcalc();
	} while(1);

	ticks1 = ticks;
	ya = 0;
	float y = 140;

	do {
		SDL_Rect rc;

 		rc.x = -xofs;
		rc.y = 0;

		SDL_BlitSurface(titleimg, NULL, videobuffer, &rc);

 		rc.x = -xofs + 320;
		rc.y = 0;

		SDL_BlitSurface(titleimg, NULL, videobuffer, &rc);

		y = y - spd * fpsr;
		for(int i = 0; i <= 26; i++) {
			int yy = y + i * 10;
			if(yy > -8 && yy < 240) {
				int x = 160 - strlen(story2[i]) * 4;
				sys_print(videobuffer, story2[i], x, yy, 4);
			}

			if(yy < 10 && i == 25) break;
		}

		int ya = 255;
		if(ticks < ticks1 + 1000) {
			ya = 255 * (ticks - ticks1) / 1000;
			if(ya < 0) ya = 0;
			if(ya > 255) ya = 255;
		}

		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, ya);
		SDL_BLITVIDEO(videobuffer, NULL, video, NULL);
		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, 255);

		SDL_Flip(video);
		SDL_PumpEvents();

		sys_tickcalc();

		float add = 0.5 * fpsr;
		if(add > 1) add = 1;
		xofs = xofs + add;
		if(xofs >= 320) xofs = xofs - 320;

		sys_pollevent();
		keys = SDL_GetKeyState(NULL);

		if(event.type == SDL_KEYDOWN) spd = 1;
		if(event.type == SDL_KEYUP) spd = 0.2;

		if(keys[config.key_menu]) break;
	} while(1);


	ticks1 = ticks;
	y = 0;

	SDL_BlitSurface(videobuffer, NULL, videobuffer2, NULL);

	do {
		if(ticks >= ticks1 + 1500) break;

		y = 255 * (ticks - ticks1) / 1500;
		if(y < 0) y = 0;
		if(y > 255) y = 255;

		SDL_FillRect(videobuffer, NULL, 0);

		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, y);
		SDL_BlitSurface(videobuffer2, NULL, videobuffer3, NULL);
		SDL_BlitSurface(videobuffer, NULL, videobuffer3, NULL);
		SDL_BLITVIDEO(videobuffer3, NULL, video, NULL);
		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, 255);

		SDL_Flip(video);
		SDL_PumpEvents();

		sys_tickcalc();
	} while(1);


	int keywait = 2000 + ticks;

	ticks1 = ticks;
	y = 0;
	do {

		SDL_BlitSurface(theendimg, NULL, videobuffer, NULL);

		y = 255;
		if(ticks < ticks1 + 1000) {
			y = 255 * (ticks - ticks1) / 1000;
			if(y < 0) y = 0;
			if(y > 255) y = 255;
		}

		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, y);
		SDL_BLITVIDEO(videobuffer, NULL, video, NULL);
		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, 255);

		SDL_Flip(video);
		SDL_PumpEvents();

		sys_tickcalc();

		sys_pollevent();
		keys = SDL_GetKeyState(NULL);

		if(event.type == SDL_KEYDOWN && keywait < ticks) break;

	} while(1);

	SDL_FillRect(videobuffer2, NULL, 0);
	SDL_FillRect(videobuffer3, NULL, 0);

	game_theend();

}

void game_eventtext(char *stri)
{
	snprintf(eventtext, sizeof(eventtext), "%s", stri);
	eventtextstart = ticks;
	eventtextend = ticks + 2500;
	itemticks = ticks + 210;
}

void game_handlewalking()
{
	if(selenemyon && monster_target && controlled_npc == 0) return;
	if(controlled_npc > 0) {
		debug_handlecontrollednpc();
		return;
	}

	unsigned int *temp/*, c*/, bgc;
	float spd, /*ppx, ppy,*/ px, py, opx, opy;
	float nx, ny, npx, npy;

	int xmax = 20 * 16 - 25;
	int ymax = 15 * 16 - 25;

	px = player.px;
	py = player.py;
	opx = px;
	opy = py;

	spd = player.walkspd * fpsr;

	nx = (px / 2 + 6);
	ny = (py / 2 + 10);

	npx = px + 12;
	npy = py + 20;
	int lx = (int)npx / 16;
	int ly = (int)npy / 16;

	int ramp = rampdata[lx][ly];
	if(ramp == 1 && movingup) spd = spd * 2;
	if(ramp == 1 && movingdown) spd = spd * 2;

	if(ramp == 2 && movingleft) movingup = 1;
	if(ramp == 2 && movingright) movingdown = 1;

	if(ramp == 3 && movingright) movingup = 1;
	if(ramp == 3 && movingleft) movingdown = 1;

	for(int x = -1; x <= 1; x++) {
		for(int y = -1; y <= 1; y++) {
			int sx = nx + x;
			int sy = ny + y;

			clipsurround[x + 1][y + 1] = 0;
			if(sx > -1 && sx < 320 && sy > -1 && sy < 192) {
				temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
				clipsurround[x + 1][y + 1] = *temp;
			}
		}
	}

	if(cheat_noclip) memset(clipsurround, 0, sizeof(clipsurround));

	if(movingup) player.walkdir = 0;
	if(movingdown) player.walkdir = 1;
	if(movingleft) player.walkdir = 2;
	if(movingright) player.walkdir = 3;

	if(movingup && clipsurround[1][0] == 0) {
		py = py - spd;
		player.walkdir = 0;
	} else if(movingup && clipsurround[1][0] > 0) {
		// move upleft
		if(movingright == 0 && clipsurround[0][0] == 0) {
			py = py - spd;
			px = px - spd;
		}

		// move upright
		if(movingleft == 0 && clipsurround[2][0] == 0) {
			py = py - spd;
			px = px + spd;
		}
	}
	if(movingdown && clipsurround[1][2] == 0) {
		py = py + spd;
		player.walkdir = 1;
	} else if(movingdown && clipsurround[1][2] > 0) {
		// move downleft
		if(movingright == 0 && clipsurround[0][2] == 0) {
			py = py + spd;
			px = px - spd;
		}

		// move downright
		if(movingleft == 0 && clipsurround[2][2] == 0) {
			py = py + spd;
			px = px + spd;
		}
	}
	if(movingleft && clipsurround[0][1] == 0) {
		px = px - spd;
		player.walkdir = 2;
	} else if(movingleft && clipsurround[0][1] > 0) {
		// move leftup
		if(movingdown == 0 && clipsurround[0][0] == 0) {
			py = py - spd;
			px = px - spd;
		}

		// move leftdown
		if(movingup == 0 && clipsurround[0][2] == 0) {
			py = py + spd;
			px = px - spd;
		}
	}
	if(movingright && clipsurround[2][1] == 0) {
		px = px + spd;
		player.walkdir = 3;
	} else if(movingright && clipsurround[2][1] > 0) {
		// move rightup
		if(movingdown == 0 && clipsurround[2][0] == 0) {
			px = px + spd;
			py = py - spd;
		}

		// move rightdown
		if(movingup == 0 && clipsurround[2][2] == 0) {
			py = py + spd;
			px = px + spd;
		}
	}

	if(px < -8) px = -8;
	if(px > xmax) px = xmax;
	if(py < -8) py = -8;
	if(py > ymax) py = ymax;

	int pass = 1;

	int sx = (px / 2 + 6);
	int sy = (py / 2 + 10);
	temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
	bgc = *temp;
	if(!cheat_noclip && bgc > 0 && bgc != 1000) {
		px = opx;
		py = opy;
		pass = 0;
	}

	// push npc
	if(pass == 1) {
		for(int i = 1; i <= lastnpc; i++) {
			if(npcinfo[i].hp > 0) {
				npx = npcinfo[i].x;
				npy = npcinfo[i].y;

				opx = npx;
				opy = npy;

				int xdif = player.px - npx;
				int ydif = player.py - npy;

				if(player.walkdir == 0) {
					if(abs(xdif) <= 8 && ydif > 0 && ydif < 8) npcinfo[i].y = npcinfo[i].y - spd;
				} else if(player.walkdir == 1) {
					if(abs(xdif) <= 8 && ydif < 0 && ydif > -8) npcinfo[i].y = npcinfo[i].y + spd;
				} else if(player.walkdir == 2) {
					if(abs(ydif) <= 8 && xdif > 0 && xdif < 8) npcinfo[i].x = npcinfo[i].x - spd;
				} else if(player.walkdir == 3) {
					if(abs(ydif) <= 8 && xdif < 0 && xdif > -8) npcinfo[i].x = npcinfo[i].x + spd;
				}

				npx = npcinfo[i].x;
				npy = npcinfo[i].y;

				sx = (int)(npx / 2 + 6);
				sy = (int)(npy / 2 + 10);
				temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
				bgc = *temp;

				if(bgc > 0) {
					npcinfo[i].x = opx;
					npcinfo[i].y = opy;
				}
			}
		}
	}

	player.opx = player.px;
	player.opy = player.py;
	player.px = px;
	player.py = py;

	if(player.px != player.opx || player.py != player.opy) player.walkframe = player.walkframe + animspd * fpsr;
	if(player.walkframe >= 16) player.walkframe = player.walkframe - 16;

	// walking over items to pickup :::
	int o = objmap[lx][ly];

	if(o > -1) {
		// fsk
		if(objectinfo[o][4] == 2 && player.inventory[INV_FLASK] < 9) {
			objmap[lx][ly] = -1;

			player.inventory[INV_FLASK]++;
			game_addFloatIcon(6, lx * 16, ly * 16);

			objmapf[curmap][lx][ly] = 1;

			game_playsound(sndpowerup);
		}

		if(objectinfo[o][5] == 7 && player.inventory[INV_DOUBLEFLASK] < 9) {
			objmap[lx][ly] = -1;

			player.inventory[INV_DOUBLEFLASK]++;
			game_addFloatIcon(12, lx * 16, ly * 16);

			objmapf[curmap][lx][ly] = 1;

			game_playsound(sndpowerup);
		}

		if(objectinfo[o][5] == 9 && player.inventory[INV_SHOCK] < 9 && (curmap == 41 && scriptflag[9][1] == 0)) {
			objmap[lx][ly] = -1;

			player.inventory[INV_SHOCK]++;
			game_addFloatIcon(17, lx * 16, ly * 16);

			objmapf[curmap][lx][ly] = 1;
			if(curmap == 41) scriptflag[9][1] = 1;

			game_playsound(sndpowerup);

		}

		if(objectinfo[o][5] == 9 && player.inventory[INV_SHOCK] < 9) {
			objmap[lx][ly] = -1;

			player.inventory[INV_SHOCK]++;
			game_addFloatIcon(17, lx * 16, ly * 16);

			objmapf[curmap][lx][ly] = 1;

			game_playsound(sndpowerup);

		}
	}
}

static const struct {
	int hp;
	int attackdelay;
	int attackdamage;
	int spelldamage;
	float walkspd;
} npc_stats[13] = {
	{0, 0, 0, 0, 0},
	{12, 2000, 2, 0, 1},
	{200, 2000, 24, 30, 1.4},
	{300, 2200, 0, 30, 1.2},
	{200, 2800, 0, 30, 1},
	{600, 2200, 50, 30, 1.3},
	{20, 1500, 0, 12, 1},
	{40, 5000, 0, 8, 1},
	{100, 1500, 0, 24, 1},
	{140, 2000, 30, 0, 1},
	{80, 1500, 24, 0, 1},
	{1200, 2000, 100, 60, 1},
	{800, 2000, 100, 50, 1}
};

int game_readmap(const char *name, int tempmap[320][200])
{
	FILE *fp = mods_fopen(name, "rb");
	char *data, *cursor;
	long size;

	if(!fp) return 0;
	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	rewind(fp);
	if(size < 0) {
		fclose(fp);
		return 0;
	}

	data = malloc((size_t)size + 1);
	if(!data) {
		fclose(fp);
		return 0;
	}
	if(fread(data, 1, (size_t)size, fp) != (size_t)size) {
		free(data);
		fclose(fp);
		return 0;
	}
	fclose(fp);
	data[size] = '\0';
	cursor = data;

	for(int x = 0; x < 320; x++) {
		for(int y = 0; y < 200; y++) {
			char *end;
			long value = strtol(cursor, &end, 10);
			if(end == cursor) {
				free(data);
				return 0;
			}
			tempmap[x][y] = (int)value;
			cursor = end;
		}
	}

	free(data);
	return 1;
}

void game_loadmap(int mapnum)
{
	controlled_npc = 0;
	selenemyon = 0;
	monster_target = 0;
	unsigned int ccc;
	SDL_Rect trect;
	FILE *fp;
	char name[256];
	int tempmap[320][200];

	ccc = SDL_MapRGB(clipbg->format, 255,255,255);

	curmap = mapnum;

	trect.x = 0;
	trect.y	= 0;
	trect.w = 320;
	trect.h = 240;
	SDL_FillRect(mapbg, &trect, 0);
	SDL_FillRect(clipbg, &trect, ccc);
	SDL_FillRect(clipbg2, &trect, ccc);

	game_setpause(0);
	cloudson = 0;
	if(mapnum < 6) cloudson = 1;
	if(mapnum > 41) cloudson = 1;
	if(mapnum > 47) cloudson = 0;
	if(mapnum == 52) cloudson = 1;
	if(mapnum == 60) cloudson = 1;
	if(mapnum == 50) cloudson = 1;
	if(mapnum == 54) cloudson = 1;
	if(mapnum == 58) cloudson = 1;
	if(mapnum == 62) cloudson = 1;
	if(mapnum == 83) cloudson = 1;

	// -----------special case
	dontdrawover = 0;
	if(mapnum == 24) dontdrawover = 1;

	if((mapnum == 53 || mapnum == 57 || mapnum == 61 || mapnum == 65 || mapnum == 62) && scriptflag[60][0] > 0) mapnum = mapnum + 100;
	if((mapnum == 161 || mapnum == 162) && scriptflag[60][0] == 2) mapnum = mapnum + 100;

	for(int i = 0; i < MAXSPELL; i++) {
		if(spellinfo[i].spellnum != 5) spellinfo[i].frame = 0;
	}

	roomlock = 0;

	// read *.map file
	sprintf(name, "mapdb/%04i.map", mapnum);
	if(!game_readmap(name, tempmap)) {
		fprintf(stderr, "Failed to load map data: %s\n", name);
		exit(1);
	}

	for(int x = 0; x <= 319; x++)
		for(int y = 0; y <= 239; y++) {
			triggerloc[x][y] = -1;
		}

	// read *.trg file
	sprintf(name, "mapdb/%04i.trg", mapnum); printf("Reading %s\n", name);
	fp = mods_fopen(name, "r");
	if(!fp) {
		fprintf(stderr, "Failed to load trigger data: %s\n", name);
		exit(1);
	}

	INPUT("%i", &ntriggers);
	if(ntriggers < 0 || ntriggers > 10000) {
		fprintf(stderr, "Invalid trigger count in %s\n", name);
		fclose(fp);
		exit(1);
	}

	for(int i = 0; i < ntriggers; i++) {
		int mapx, mapy, trig;

		INPUT("%i", &mapx);
		INPUT("%i", &mapy);
		INPUT("%i", &trig);

		if(mapx < 0 || mapx >= 320 || mapy < 0 || mapy >= 240) {
			fprintf(stderr, "Invalid trigger position in %s\n", name);
			fclose(fp);
			exit(1);
		}
		triggerloc[mapx][mapy] = trig;
	}
	fclose(fp);


	for(int y = 0; y <= 23; y++)
		for(int x = 0; x <= 39; x++)
			rampdata[x][y] = tempmap[3 * 40 + x][y + 40];


	for(int y = 0; y <= 23; y++) 
		for(int x = 0; x <= 39; x++)
			for(int l = 0; l <= 2; l++)
				for(int a = 0; a <= 2; a++)
					tileinfo[l][x][y][a] = 0;

	if(scriptflag[4][0] == 1 && curmap == 4) {
		triggerloc[9][7] = 5004;
		tempmap[9][7] = 41;
		tempmap[9][7 + 40] = 0;
	}

	for(int y = 0; y <= 23; y++) {
		for(int x = 0; x <= 39; x++) {
			for(int l = 0; l <= 2; l++) {
				int lx, ly, curtile, curtilelayer;
				int curtilel, curtilex, curtiley;
				int ffa, ffb;

				ly = y;
				lx = x + l * 40;

				// tile
				curtile = tempmap[lx][ly];
				curtilelayer = tempmap[lx][ly + 40];

				if(curtile > 0) {
					curtile = curtile - 1;
					curtilel = curtilelayer;
					curtilex = curtile % 20;
					curtiley = (curtile - curtilex) / 20;

					tileinfo[l][x][y][0] = curtile + 1;
					tileinfo[l][x][y][1] = curtilelayer;

					rcSrc.x = curtilex * 16;
					rcSrc.y = curtiley * 16;
					rcSrc.w = 16;
					rcSrc.h = 16;

					rcDest.x = x * 16;
					rcDest.y = y * 16;
					rcDest.w = 16;
					rcDest.h = 16;

					if(l == 2 && curtilel == 1) {
						for(int ff = 0; ff <= 5; ff++) {
							ffa = 20 * 5 - 1 + ff * 20;
							ffb = 20 * 5 + 4 + ff * 20;
							if(curtile > ffa && curtile < ffb) {
								SDL_SetAlpha(tiles[curtilel], SDL_SRCALPHA, 128);
							}
						}
					}
					if(l == 1 && curtilel == 2) {
						for(int ff = 0; ff <= 4; ff++) {
							ffa = 20 * (5 + ff) + 3;
							if(curtile == ffa) {
								SDL_SetAlpha(tiles[curtilel], SDL_SRCALPHA, 192);
							}
						}
					}

					SDL_BlitSurface(tiles[curtilel], &rcSrc, mapbg, &rcDest);
					SDL_SetAlpha(tiles[curtilel], SDL_SRCALPHA, 255);

					rcDest.x = x * 8;
					rcDest.y = y * 8;
					rcDest.w = 8;
					rcDest.h = 8;

					SDL_FillRect(clipbg, &rcDest, 0);
				}

			}
		}
	}


	for(int x = 0; x <= 39; x++) {
		for(int y = 0; y <= 23; y++) {

			int d = tempmap[3 * 40 + x][y];

			int clip = 0;
			int npc = 0;

			if(scriptflag[4][0] == 1 && x == 9 && y == 7) d = 99;

			if(d > 0) {
				clip = d % 2;
				d = (d - clip) / 2;
				npc = d % 2;
				d = (d - npc) / 2;

				if(d == 99 && x == 9 && y == 7) clip = 1;

				if(clip) {
					if(d != 99) d = tempmap[6 * 40 + x][y];
					if(d == 99) d = 1;

					int x1 = x * 8;
					int y1 = y * 8;

					if(d == 1) {
						for(int i = 0; i <= 7; i++) {
							sys_line(clipbg, x1, y1 + i, x1 + 7 - i, y1 + i, ccc);
						}
					} else if(d == 2) {
						sys_line(clipbg, x1, y1, x1 + 7, y1, ccc);
						sys_line(clipbg, x1, y1 + 1, x1 + 7, y1 + 1, ccc);
					} else if(d == 3) {
						for(int i = 0; i <= 7; i++) {
							sys_line(clipbg, x1 + i, y1 + i, x1 + 7, y1 + i, ccc);
						}
					} else if(d == 4) {
						sys_line(clipbg, x1, y1, x1, y1 + 7, ccc);
						sys_line(clipbg, x1 + 1, y1, x1 + 1, y1 + 7, ccc);
					} else if(d == 5) {
						rcDest.x = x1;
						rcDest.y = y1;
						rcDest.w = 8;
						rcDest.h = 8;
						SDL_FillRect(clipbg, &rcDest, SDL_MapRGB(clipbg->format, 255,255,255));
					} else if(d == 6) {
						sys_line(clipbg, x1 + 7, y1, x1 + 7, y1 + 7, ccc);
						sys_line(clipbg, x1 + 6, y1, x1 + 6, y1 + 7, ccc);
					} else if(d == 7) {
						for(int i = 0; i <= 7; i++) {
							sys_line(clipbg, x1, y1 + i, x1 + i, y1 + i, ccc);
						}
					} else if(d == 8) {
						sys_line(clipbg, x1, y1 + 7, x1 + 7, y1 + 7, ccc);
						sys_line(clipbg, x1, y1 + 7, x1 + 6, y1 + 6, ccc);
					} else if(d == 9) {
						for(int i = 0; i <= 7; i++) {
							sys_line(clipbg, x1 + 7 - i, y1 + i, x1 + 7, y1 + i, ccc);
						}
					}
				}
			}
		}
	}

	lastobj = 0;
	lastnpc = 0;

	for(int i = 0; i < MAXNPC; i++)
		npcinfo[i].onmap = 0;

	for(int x = 0; x <= 19; x++) {
		for(int y = 0; y <= 14; y++) {
			int d = tempmap[3 * 40 + x][y];

			int clip = 0;
			int npc = 0;
			int obj = 0;
			if(d > 0) {
				clip = d % 2;
				d = (d - clip) / 2;
				npc = d % 2;
				d = (d - npc) / 2;
				obj = d % 2;
			}

			objmap[x][y] = -1;

			if(obj == 1) {

				int o = tempmap[5 * 40 + x][y];

				if(objmapf[curmap][x][y] == 0) {
					objmap[x][y] = o;

					if(objectinfo[o][0] > 1) {
						if(o > lastobj) lastobj = o;
					}

					int x1 = x * 8;
					int y1 = y * 8;

					rcDest.x = x1;
					rcDest.y = y1;
					rcDest.w = 8;
					rcDest.h = 8;

					if(objectinfo[o][4] == 1) SDL_FillRect(clipbg, &rcDest, SDL_MapRGB(clipbg->format, 255,255,255));
					if(objectinfo[o][4] == 3) SDL_FillRect(clipbg, &rcDest, SDL_MapRGB(clipbg->format, 255,255,255));
				}
			}
			if(npc == 1) {
				int o = tempmap[4 * 40 + x][y];

				if(o > lastnpc) lastnpc = o;

				npcinfo[o].x = x * 16 - 4;
				npcinfo[o].y = y * 16 - 5;

				npcinfo[o].walkdir = 1;
				npcinfo[o].onmap = 1;
			}
		}
	}


	if(curmap == 62 && scriptflag[8][0] > 0) lastnpc = 0;
	if(curmap == 73 && scriptflag[12][0] > 0) lastnpc = 0;
	if(curmap == 81 && scriptflag[13][0] > 0) lastnpc = 0;

	if(curmap == 73 && scriptflag[12][0] == 0) roomlock = 1;
	if(curmap == 81 && scriptflag[13][0] == 0) roomlock = 1;
	if(curmap == 83 && scriptflag[15][0] == 0) roomlock = 1;
	if(curmap == 82) roomlock = 1;

	// read *.npc file
	sprintf(name, "mapdb/%04i.npc", mapnum);
	fp = mods_fopen(name, "r");
	if(!fp) {
		fprintf(stderr, "Failed to load NPC data: %s\n", name);
		exit(1);
	}

	for(int i = 0; i < MAXNPC; i++) {
		INPUT("%i", &npcinfo[i].spriteset);
		INPUT("%i", &npcinfo[i].x1);
		INPUT("%i", &npcinfo[i].y1);
		INPUT("%i", &npcinfo[i].x2);
		INPUT("%i", &npcinfo[i].y2);
		INPUT("%i", &npcinfo[i].movementmode);
		INPUT("%i", &npcinfo[i].hp);
		INPUT("%i", &npcinfo[i].item1);
		INPUT("%i", &npcinfo[i].item2);
		INPUT("%i", &npcinfo[i].item3);
		INPUT("%i", &npcinfo[i].script);

		int spriteset = npcinfo[i].spriteset;
		if(spriteset >= 1 && spriteset <= 12) {
			npcinfo[i].hp = npc_stats[spriteset].hp;
			npcinfo[i].attackdelay = npc_stats[spriteset].attackdelay;
			npcinfo[i].attackdamage = npc_stats[spriteset].attackdamage;
			npcinfo[i].spelldamage = npc_stats[spriteset].spelldamage;
			npcinfo[i].walkspd = npc_stats[spriteset].walkspd;
		}

		if(spriteset == 2 || spriteset == 9) npcinfo[i].swayspd = 1;
		if(spriteset == 2) npcinfo[i].castpause = game_time;
		if(spriteset == 9) npcinfo[i].castpause = 0;
		if(spriteset == 5) npcinfo[i].swayangle = 0;
		if(spriteset >= 10) npcinfo[i].floating = RND() * 16;

		if(config.random_enemies && (spriteset == 1 || spriteset == 6 || spriteset == 8) &&
			(int)(RND() * 5) == 0) npcinfo[i].hp = 0;
		if(config.random_enemies && spriteset == 7 && (int)(RND() * 8) == 0) npcinfo[i].hp = 0;

		if(npcinfo[i].onmap == 0) npcinfo[i].hp = 0;

		npcinfo[i].maxhp = npcinfo[i].hp;
		npcinfo[i].spelldamagecarry = 0;

		npcinfo[i].attacking = 0;
		npcinfo[i].attackframe = 0;
		npcinfo[i].cattackframe = 0;
		npcinfo[i].attackspd = 1.5;
		npcinfo[i].attacknext = game_time + npcinfo[i].attackdelay * (1 + RND() * 2);

		if(npcinfo[i].spriteset == 2 || npcinfo[i].spriteset == 9) {
			npcinfo[i].bodysection[0].sprite = 0;
			npcinfo[i].bodysection[1].sprite = 1;
			npcinfo[i].bodysection[2].sprite = 2;
			npcinfo[i].bodysection[3].sprite = 3;
			npcinfo[i].bodysection[4].sprite = 4;
			npcinfo[i].bodysection[5].sprite = 3;
			npcinfo[i].bodysection[6].sprite = 3;
			npcinfo[i].bodysection[7].sprite = 5;

			npcinfo[i].bodysection[0].bonelength = 8;
			npcinfo[i].bodysection[1].bonelength = 7;
			npcinfo[i].bodysection[2].bonelength = 6;
			npcinfo[i].bodysection[3].bonelength = 4;
			npcinfo[i].bodysection[4].bonelength = 4;
			npcinfo[i].bodysection[5].bonelength = 4;
			npcinfo[i].bodysection[6].bonelength = 4;

			for(int f = 0; f <= 7; f++) {
				npcinfo[i].bodysection[f].x = npcinfo[i].x + 12;
				npcinfo[i].bodysection[f].y = npcinfo[i].y + 14;
			}

			npcinfo[i].headtargetx[0] = npcinfo[i].x + 12;
			npcinfo[i].headtargety[0] = npcinfo[i].y + 14;

		}

		if(npcinfo[i].spriteset == 5) {
			for(int f = 0; f <= 29; f++) {
				npcinfo[i].bodysection[f].x = npcinfo[i].x + 12;
				npcinfo[i].bodysection[f].y = npcinfo[i].y + 14;
			}

			for(int f = 0; f <= 2; f++) {
				npcinfo[i].headtargetx[f] = npcinfo[i].x + 12;
				npcinfo[i].headtargety[f] = npcinfo[i].y + 14;

				npcinfo[i].attacking2[f] = 0;
				npcinfo[i].attackframe2[f] = 0;
			}
		}

		if(npcinfo[i].script == 2) {
			roomlock = 1;
			if(scriptflag[2][0] > 0) {
				roomlock = 0;
				npcinfo[i].hp = 0;
			}
		}

		if(npcinfo[i].script == 3) {
			roomlock = 1;
			if(scriptflag[3][0] > 0) {
				roomlock = 0;
				npcinfo[i].hp = 0;
			}
		}

		if(npcinfo[i].script == 5) {
			roomlock = 1;
			if(scriptflag[5][0] > 0) {
				roomlock = 0;
				npcinfo[i].hp = 0;
			}
		}

		if(npcinfo[i].script == 15) {
			roomlock = 1;
			if(scriptflag[15][0] > 0) {
				roomlock = 0;
				npcinfo[i].hp = 0;
			}
		}

		npcinfo[i].pause = game_time;
	}

	fclose(fp);


	// academy master key
	if(curmap == 34 && scriptflag[2][0] == 1) items_placeobject(9, 7, 5);
	// academy crystal
	if(curmap == 24 && player.foundspell[0] == 0 && scriptflag[3][0] == 1) items_placeobject(9, 7, 6);
	// gardens master key
	if(curmap == 62 && scriptflag[8][0] == 1) items_placeobject(13, 7, 5);
	// gardens fidelis sword
	if(curmap == 66 && scriptflag[5][0] == 1 && player.sword == 1) items_placeobject(9, 6, 9);
	// citadel armour
	if(curmap == 73 && scriptflag[12][0] == 1 && player.armour == 1) items_placeobject(8, 7, 16);
	// citadel master key
	if(curmap == 81 && scriptflag[13][0] == 1) items_placeobject(11, 10, 5);
	// max ups
	if(curmap == 83 && scriptflag[15][0] == 1 && player.sword < 3) items_placeobject(6, 8, 18);
	if(curmap == 83 && scriptflag[16][0] == 1 && player.shield < 3) items_placeobject(9, 8, 19);
	if(curmap == 83 && scriptflag[17][0] == 1 && player.armour < 3) items_placeobject(12, 8, 20);

	SDL_BlitSurface(clipbg, NULL, clipbg2, NULL);
}

void game_main()
{
	game_showlogos();
	game_title(0);
	game_saveloadnew();
}

void game_newgame()
{
	debug_resetcheats();
	float xofs = 0;
	float ld = 0, add;
	int cnt = 0;

	SDL_FillRect(videobuffer2, NULL, 0);
	SDL_FillRect(videobuffer3, NULL, 0);

	SDL_BlitSurface(videobuffer, NULL, videobuffer3, NULL);
	SDL_BlitSurface(videobuffer, NULL, videobuffer2, NULL);

	fpsr = 0.0;
	int y = 140;

	if(menabled == 1 && config.music == 1) {
		Mix_HaltChannel(-1);
		musicchannel = Mix_PlayChannel(-1, mendofgame, -1);
		Mix_Volume(musicchannel, 0);
	}

	secsingame = 0;
	secstart = 0;
	game_time = 0;

	int ldstop = 0;

	do {
		SDL_Rect rc;

		ld += 4 * fpsr;
		if((int)ld > config.musicvol) ld = config.musicvol;
		if(menabled == 1 && ldstop == 0) {
			Mix_Volume(musicchannel, (int)ld);
			if((int)ld == config.musicvol) ldstop = 1;
		}

 		rc.x = -xofs;
		rc.y = 0;

		SDL_BlitSurface(titleimg, NULL, videobuffer, &rc);

 		rc.x = -xofs + 320;
		rc.y = 0;

		SDL_BlitSurface(titleimg, NULL, videobuffer, &rc);

		if(++cnt >= 6) {
			cnt = 0;
			y--;
		}

		for(int i = 0; i <= 47; i++) {
			int yy, x;

			yy = y + i * 10;
			if(yy > -8 && yy < 240) {
				x = 160 - strlen(story[i]) * 4;
				sys_print(videobuffer, story[i], x, yy, 4);
			}

			if(yy < 10 && i == 47) goto __exit_do;
		}

		SDL_BLITVIDEO(videobuffer, NULL, video, NULL);
		SDL_Flip(video);

		sys_tickcalc();

		add = 0.5 * fpsr;
		if(add > 1) add = 1;
		xofs += add;
		if(xofs >= 320) xofs = xofs - 320;

		sys_pollevent();
		keys = SDL_GetKeyState(NULL);

		if(event.type == SDL_KEYDOWN) cnt = 6;
		if(keys[config.key_menu]) goto __exit_do;

		} while(1);
__exit_do:

	player.px = 0;
	player.py = 0;
	player.opx = 0;
	player.opy = 0;
	player.walkdir = 0;
	player.walkframe = 0;
	player.walkspd = 0;
	player.attackframe = 0;
	player.attackspd = 0;
	player.hp = 0;
	player.maxhp = 0;
	player.hpflash = 0;
	player.level = 0;
	player.maxlevel = 0;
	player.sword = 0;
	player.shield = 0;
	player.armour = 0;
	for(int i = 0; i < 5; i++) {
		player.foundspell[i] = 0;
		player.spellcharge[i] = 0;
		player.inventory[i] = 0;
	}
	player.attackstrength = 0;
	player.spelldamage = 0;
	player.sworddamage = 0;
	player.exp = 0;
	player.nextlevel = 0;

	memset(scriptflag, 0, sizeof(scriptflag));
	memset(objmapf, 0, sizeof(objmapf));
	memset(roomlocks, 0, sizeof(roomlocks));

	roomlocks[66] = 2;
	roomlocks[24] = 2;
	roomlocks[17] = 1;
	roomlocks[34] = 1;
	roomlocks[50] = 1;
	roomlocks[73] = 1;
	roomlocks[82] = 2;

	player.walkspd = 1.1;
	animspd = 0.5;
	attacking = 0;
	player.attackspd = 1.5;

	player.sword = 1;
	player.level = 1;
	player.maxlevel = 22;
	player.nextlevel = 50;
	player.shield = 1;
	player.armour = 1;
	player.hp = 14;
	player.maxhp = player.hp;

	player.sworddamage = player.level * 2;
	player.spelldamage = player.level * 3 / 2;

	player.px = 15*16 - 4;
	player.py = 6*16 - 4;
	player.walkdir = 1;

	pgardens = 0;
	ptown = 0;
	pboss = 0;
	pacademy = 0;
	pcitadel = 0;

	game_loadmap(2);

	game_playgame();
}

void game_playgame()
{
	game_swash();

	if(pmenu == 1 && menabled == 1) {
		Mix_HaltChannel(menuchannel);
		pmenu = 0;
		Mix_Resume(musicchannel);
	}

	do {
		if(!windowfocused) {
			sys_pollevent();
			SDL_Flip(video);
			sys_tickcalc();
			continue;
		}

		if(forcepause == 0) {
			game_updanims();
			game_updnpcs();
		}

		game_checktrigger();
		game_checkinputs();

		if(forcepause == 0 || spells_lightningactive()) game_handlewalking();

		game_updatey();
		game_drawview();

		game_updmusic();

		sys_update();
	} while(1);
}

void game_processtrigger(int trignum)
{
	int trigtype, tx, ty, tmap, tjumpstyle, tsx, tsy;

	trigtype = triggers[trignum][0];

	if(roomlock == 1) return;
	// map jump------------------------------
	if(trigtype == 0) {
		tx = triggers[trignum][1];
		ty = triggers[trignum][2];
		tmap = triggers[trignum][3];
		tjumpstyle = triggers[trignum][4];

		if(roomlocks[tmap] > 0) {
			if(saidlocked == 0) game_eventtext("Locked");
			saidlocked = 1;
			canusekey = 1;
			locktype = roomlocks[tmap];
			roomtounlock = tmap;
			return;
		}

		if(tmap == 1) {
			if(saidjammed == 0) game_eventtext("Door Jammed!");
			saidjammed = 1;
			return;
		}

		saidlocked = 0;
		saidjammed = 0;

		// loc-sxy+oldmaploc
		if(tjumpstyle == 0) {

			tsx = triggers[trignum][5];
			tsy = triggers[trignum][6];

			player.px += (tx - tsx) * 16;
			player.py += (ty - tsy) * 16;

			// HACKFIX
			if(player.px < 0) player.px = 0;
			if(player.py < 0) player.py = 0;

			if(tmap > 0) {
				game_playsound(snddoor);

				game_loadmap(tmap);
				game_swash();
			}
		}
	}

	for(int i = 0; i < MAX_FLOATING_TEXTS; i++) {
		floattext[i][0] = 0;
		floaticon[i][0] = 0;
	}
}

void game_saveloadnew()
{

	itemselon = 0;
	float y = 0; int yy;
	int currow, curcol, lowerlock;
	int confirm_overwrite = 0;
	int ticks1, tickpause;
	PLAYERTYPE savepreview[4];
	int savepreviewtime[4];

	for(int i = 0; i < 4; i++) {
		state_load_player(i);
		savepreview[i] = playera;
		savepreviewtime[i] = asecstart;
	}

	clouddeg = 0;

	SDL_SetAlpha(videobuffer, SDL_SRCALPHA, 255);
	SDL_SetAlpha(saveloadimg, SDL_SRCALPHA, 192);

	currow = 0;
	curcol = 0;

	lowerlock = 0;

	ticks1 = ticks;
	tickpause = ticks + 150;

	do {
		SDL_FillRect(videobuffer, NULL, 0);

		y = y + 1 * fpsr;

		rcDest.x = 256 + 256 * cos(3.141592 / 180 * clouddeg * 40);
		rcDest.y = 192 + 192 * sin(3.141592 / 180 * clouddeg * 40);
		rcDest.w = 320;
		rcDest.h = 240;

		SDL_SetAlpha(cloudimg, SDL_SRCALPHA, 128);
		SDL_BlitSurface(cloudimg, &rcDest, videobuffer, NULL);
		SDL_SetAlpha(cloudimg, SDL_SRCALPHA, 64);


		rcDest.x = 256;
		rcDest.y = 192;
		rcDest.w = 320;
		rcDest.h = 240;

		SDL_SetAlpha(cloudimg, SDL_SRCALPHA, 128);
		SDL_BlitSurface(cloudimg, &rcDest, videobuffer, NULL);
		SDL_SetAlpha(cloudimg, SDL_SRCALPHA, 64);

		SDL_BlitSurface(saveloadimg, NULL, videobuffer, NULL);

		sys_pollevent();
		keys = SDL_GetKeyState(NULL);

		if(tickpause < ticks) {
			if(event.type == SDL_KEYDOWN) {
				itemticks = ticks + 220;

				if(keys[config.key_attack] || keys[SDLK_RETURN]) {
					if(currow == 0 && curcol == 4) exit(0);
					// RETURN
					if(currow == 0 && curcol == 3) {
						// reset keys to avoid returning
						keys[SDLK_RETURN] = keys[SDLK_SPACE] = 0;
						return;
					}
					// NEW GAME
					if(currow == 0 && curcol == 0) game_newgame();

					// LOAD GAME
					if(currow == 0 && curcol == 1) {
						lowerlock = 1;
						currow = 1 + saveslot;
						tickpause = ticks + 125;
					}
					// SAVE GAME
					if(currow == 0 && curcol == 2) {
						lowerlock = 1;
						currow = 1;
						tickpause = ticks + 125;
					}


					if(lowerlock == 1 && curcol == 1 && tickpause < ticks) {
						if(player.level == 0 && savepreview[currow - 1].level > 0 && !confirm_overwrite) {
							confirm_overwrite = 1;
						} else if(state_save(currow - 1)) {
							secstart = secstart + secsingame;
							secsingame = 0;
							lowerlock = 0;
							confirm_overwrite = 0;
							saveslot = currow - 1;
							state_load_player(saveslot);
							savepreview[saveslot] = playera;
							savepreviewtime[saveslot] = asecstart;
							currow = 0;
						}
					}

					if(lowerlock == 1 && curcol == 2 && tickpause < ticks) {
						if(state_load(currow - 1)) {
							debug_resetcheats();
							player.walkspd = 1.1;
							animspd = .5;
							attacking = 0;
							player.attackspd = 1.5;

							pgardens = 0;
							ptown = 0;
							pboss = 0;
							pacademy = 0;
							pcitadel = 0;

							Mix_HaltChannel(-1);

							secsingame = 0;
							saveslot = currow - 1;
							game_loadmap(curmap);
							game_playgame();
						}
					}

					tickpause = ticks + 125;
				}

				switch(event.key.keysym.sym) {
				case SDLK_ESCAPE:
					game_setpause(0);
					if(lowerlock == 0) return;
					lowerlock = 0;
					confirm_overwrite = 0;
					currow = 0;
					tickpause = ticks + 125;
					break;
				case SDLK_DOWN:
					if(lowerlock == 1 && !confirm_overwrite) {
						currow = currow + 1;
						if(currow == 5) currow = 1;
						tickpause = ticks + 125;
					}
					break;

				case SDLK_UP:
					if(lowerlock == 1 && !confirm_overwrite) {
						currow = currow - 1;
						if(currow == 0) currow = 4;
						tickpause = ticks + 125;
					}
					break;

				case SDLK_LEFT:
					if(lowerlock == 0) {
						curcol = curcol - 1;
						if(curcol == -1) curcol = 3;
						tickpause = ticks + 125;
					}
					break;

				case SDLK_RIGHT:
					if(lowerlock == 0) {
						curcol = curcol + 1;
						if(curcol == 4) curcol = 0;
						tickpause = ticks + 125;
					}
					break;
				default:
					;
				}
			}
		}


		// savestates---------------------------------

		for(int ff = 0; ff <= 3; ff++) {
			playera = savepreview[ff];
			asecstart = savepreviewtime[ff];

			if(playera.level > 0) {
				char line[256];
				int sx, sy, cc, ss, nx;

				sx = 8;
				sy = 57 + ff * 48;

				// time
				int ase = asecstart;
				int h = ((ase - (ase % 3600)) / 3600);
				ase = (ase - h * 3600);
				int m = ((ase - (ase % 60)) / 60);
				int s = (ase - m * 60);

				sprintf(line, "Game Time: %02i:%02i:%02i", h, m, s);
				sys_print(videobuffer, line, 160 - strlen(line) * 4, sy, 0);

				sx  = 12;
				sy = sy + 11;
				cc = 0;

				sprintf(line, "Health: %i/%i", playera.hp, playera.maxhp);
				sys_print(videobuffer, line, sx, sy, cc);

				if(playera.level == 22)
					strcpy(line, "Level: MAX");
				else
					sprintf(line, "Level: %i", playera.level);

				sys_print(videobuffer, line, sx, sy + 11, 0);

				rcSrc.x = sx + 15 * 8 + 24;
				rcSrc.y = sy + 1;

				ss = (playera.sword - 1) * 3;
				if(playera.sword == 3) ss = 18;
				SDL_BlitSurface(itemimg[ss], NULL, videobuffer, &rcSrc);

				rcSrc.x = rcSrc.x + 16;
				ss = (playera.shield - 1) * 3 + 1;
				if(playera.shield == 3) ss = 19;
				SDL_BlitSurface(itemimg[ss], NULL, videobuffer, &rcSrc);

				rcSrc.x = rcSrc.x + 16;
				ss = (playera.armour - 1) * 3 + 2;
				if(playera.armour == 3) ss = 20;
				SDL_BlitSurface(itemimg[ss], NULL, videobuffer, &rcSrc);

				nx = rcSrc.x + 13 + 3 * 8;
				rcSrc.x = nx - 17;

				if(playera.foundspell[0] == 1) {
					for(int i = 0; i < 5; i++) {
						rcSrc.x = rcSrc.x + 17;
						if(playera.foundspell[i] == 1) SDL_BlitSurface(itemimg[7 + i], NULL, videobuffer, &rcSrc);
					}
				}
			} else {
				int sy = 57 + ff * 48;
				sys_print(videobuffer, "Empty", 160 - 5 * 4, sy, 0);
			}
		}
		// ------------------------------------------


		if(currow == 0) {
			rcDest.y = 18;
			if(curcol == 0) rcDest.x = 10;
			if(curcol == 1) rcDest.x = 108;
			if(curcol == 2) rcDest.x = 170;
			if(curcol == 3) rcDest.x = 230;
			rcDest.x = (float)(rcDest.x + 2 + 2 * sin(3.14159 * 2 * itemyloc / 16));
		}

		if(currow > 0) {
			rcDest.x = (float)(0 + 2 * sin(3.14159 * 2 * itemyloc / 16));
			rcDest.y = (float)(53 + (currow - 1) * 48);
		}

		SDL_BlitSurface(itemimg[15], NULL, videobuffer, &rcDest);


		if(lowerlock == 1) {
			rcDest.y = 18;
			if(curcol == 1) rcDest.x = 108;
			if(curcol == 2) rcDest.x = 170;
			rcDest.x = rcDest.x; // + 2 + 2 * sin(-3.14159 * 2 * itemyloc / 16)

			SDL_BlitSurface(itemimg[15], NULL, videobuffer, &rcDest);
		}

		if(confirm_overwrite) {
			game_fillrect(videobuffer, 40, 100, 240, 48, 0);
			sys_print(videobuffer, "OVERWRITE THIS SAVE?", 80, 108, 4);
			sys_print(videobuffer, "CTRL/ENTER: CONFIRM", 84, 122, 0);
			sys_print(videobuffer, "ESC: CANCEL", 108, 134, 0);
		}

		yy = 255;
		if(ticks < ticks1 + 1000) {
			yy = 255 * (ticks - ticks1) / 1000;
			if(yy < 0) yy = 0;
			if(yy > 255) yy = 255;
		}

		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, (int)yy);
		SDL_BLITVIDEO(videobuffer, NULL, video, NULL);
		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, 255);

		SDL_Flip(video);
		SDL_PumpEvents();

		sys_tickcalc();

		clouddeg += 0.01 * fpsr;
		while(clouddeg >= 360) clouddeg -= 360;

		itemyloc += 0.6 * fpsr;
		while(itemyloc >= 16) itemyloc -= 16;

		} while(1);

	SDL_SetAlpha(cloudimg, SDL_SRCALPHA, 64);
}

void game_showlogos()
{
	//printf("SHOWLOGOS: %i\n", config.showlogos);
	if (config.showlogos == 0) {
		return;
	}

	float y;
	int ticks1;

	ticks1 = ticks;

	y = 0.0;

	do {
		y = 255.0;
		if(ticks < ticks1 + 1000) {
			y = 255.0 * ((float)(ticks - ticks1) / 1000.0);
			if(y < 0.0) y = 0.0;
			if(y > 255.0) y = 255.0;
		}

		if(ticks > ticks1 + 3000) {
			y = 255.0 - 255.0 * ((float)(ticks - ticks1 - 3000.0) / 1000.0);
			if(y < 0.0) y = 0.0;
			if(y > 255.0) y = 255.0;
		}

		SDL_FillRect(videobuffer, NULL, 0);

		SDL_SetAlpha(logosimg, SDL_SRCALPHA, (int)y);
		SDL_BlitSurface(logosimg, NULL, videobuffer, NULL);
		SDL_SetAlpha(logosimg, SDL_SRCALPHA, 255);

		SDL_BLITVIDEO(videobuffer, NULL, video, NULL);

		SDL_Flip(video);
		SDL_PumpEvents();

		sys_tickcalc();

			if(ticks > ticks1 + 4000) break;
	} while(1);
}


void game_swash()
{
	float y;

	if(config.fastfade) {
		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, 255);
		return;
	}

	y = 0;

	do {
		y = y + 1 * fpsr;

		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, (int)y);
		SDL_FillRect(videobuffer, NULL, 0);
		SDL_BLITVIDEO(videobuffer, NULL, video, NULL);

		SDL_Flip(video);
		SDL_PumpEvents();

		sys_tickcalc();

		clouddeg += 0.01 * fpsr;
		while(clouddeg >= 360) clouddeg = clouddeg - 360;

		if(y > 10) break;
	} while(1);

	y = 0;
	do {
		y = y + 1 * fpsr;

		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, (int)(y * 25));
		SDL_BlitSurface(mapbg, NULL, videobuffer, NULL);

		if(cloudson == 1) {
			rcDest.x = (float)(256 + 256 * cos(3.141592 / 180 * clouddeg));
			rcDest.y = (float)(192 + 192 * sin(3.141592 / 180 * clouddeg));
			rcDest.w = 320;
			rcDest.h = 240;

			SDL_BlitSurface(cloudimg, &rcDest, videobuffer, NULL);
		}

		SDL_BLITVIDEO(videobuffer, NULL, video, NULL);
		SDL_Flip(video);
		SDL_PumpEvents();

		sys_tickcalc();

		clouddeg += 0.01 * fpsr;
		while(clouddeg >= 360) clouddeg = clouddeg - 360;

		if(y > 10) break;
	} while(1);


	SDL_SetAlpha(videobuffer, SDL_SRCALPHA, 255);
}

void game_theend()
{
	for(int i = 0; i < MAX_FLOATING_TEXTS; i++) {
		floattext[i][0] = 0;
		floaticon[i][0] = 0;
	}

	for(float y = 0; y < 100; y += fpsr) {
		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, (int)y);
		SDL_FillRect(videobuffer, NULL, 0);
		SDL_BLITVIDEO(videobuffer, NULL, video, NULL);

		SDL_Flip(video);
		SDL_PumpEvents();

		sys_tickcalc();
	}

	game_title(0);
}

static void game_modsmenu(void)
{
	int selected = 0, keypause = ticks + 200;
	for(int i = 0; i < mods_count(); i++) {
		if(strcasecmp(mods_name(i), mods_current()) == 0) selected = i;
	}

	for(;;) {
		SDL_BlitSurface(titleimg, NULL, videobuffer, NULL);
		game_fillrect(videobuffer, 48, 28, 224, 184, 0);
		sys_print(videobuffer, "MODS", 144, 36, 3);
		sys_print(videobuffer, (char *)mods_current(), 160 - 4 * strlen(mods_current()), 50, 4);

		int first = selected / 12 * 12;
		for(int row = 0; row < 12 && first + row < mods_count(); row++) {
			int index = first + row;
			if(index == selected) sys_print(videobuffer, ">", 56, 66 + row * 10, 0);
			sys_print(videobuffer, (char *)mods_name(index), 72, 66 + row * 10, 0);
		}
		sys_print(videobuffer, "ENTER: LOAD  ESC: BACK", 64, 196, 0);
		SDL_BLITVIDEO(videobuffer, NULL, video, NULL);
		SDL_Flip(video);
		sys_tickcalc();
		sys_pollevent();

		if(event.type == SDL_KEYDOWN && keypause < ticks) {
			int key = event.key.keysym.sym;
			keypause = ticks + 130;
			if(key == config.key_menu) break;
			if(key == config.key_up) selected = (selected + mods_count() - 1) % mods_count();
			if(key == config.key_down) selected = (selected + 1) % mods_count();
			if(key == config.key_left) selected = (selected + mods_count() - 12) % mods_count();
			if(key == config.key_right) selected = (selected + 12) % mods_count();
			if(key == config.key_attack || key == SDLK_RETURN) mods_launch(selected);
		}
	}
	itemticks = ticks + 250;
}

void game_title(int mode)
{
	float xofs = 0;
	int ticks1, keypause;
	int cursel, ldstop;
	int x, y;

	rcSrc.x = 0;
	rcSrc.y = 0;
	rcSrc.w = 320;
	rcSrc.h = 240;

	SDL_FillRect(videobuffer2, &rcSrc, 0);
	SDL_FillRect(videobuffer3, &rcSrc, 0);

	SDL_BlitSurface(videobuffer, NULL, videobuffer3, NULL);
	SDL_BlitSurface(videobuffer, NULL, videobuffer2, NULL);

	cursel = 0;

	keypause = ticks + 220;

	ticks1 = ticks;

	if(menabled == 1 && config.music == 1) {
		Mix_Volume(musicchannel, 0);
		Mix_Pause(musicchannel);

		menuchannel = Mix_PlayChannel(-1, mmenu, -1);
		Mix_Volume(menuchannel, config.musicvol);
		pmenu = 1;
	}

	ldstop = 0;

	float ld = 0;
	do {
		SDL_Rect rc;

		ld += 4.0 * fpsr;
		if(ld > config.musicvol) ld = config.musicvol;
		if(menabled == 1 && ldstop == 0) {
			Mix_Volume(menuchannel, (int)ld);
			if((int)ld == config.musicvol) ldstop = 1;
		}

		rc.x = -xofs;
		rc.y = 0;

		SDL_BlitSurface(titleimg, NULL, videobuffer, &rc);

		rc.x = -xofs + 320.0;
		rc.y = 0;

		SDL_BlitSurface(titleimg, NULL, videobuffer, &rc);

		rc.x = 0;
		rc.y = 0;

		SDL_BlitSurface(titleimg2, NULL, videobuffer, &rc);

		y = 160;
		x = 160 - 14 * 4;

		sys_print(videobuffer, "new game/save/load", x, y, 4);
		sys_print(videobuffer, "options", x, y + 16, 4);
		sys_print(videobuffer, "mods", x, y + 32, 4);
		sys_print(videobuffer, "quit game", x, y + 48, 4);

		if(mode == 1) sys_print(videobuffer, "return", x, y + 64, 4);
		else sys_print(videobuffer, "(c) 2005 by Daniel 'Syn9' Kennedy", 28, 224, 4);

		rc.x = (float)(x - 16 - 4 * cos(3.14159 * 2 * itemyloc / 16));
		rc.y = (float)(y - 4 + 16 * cursel);

		SDL_BlitSurface(itemimg[15], NULL, videobuffer, &rc);

		float yf = 255.0;
		if(ticks < ticks1 + 1000) {
			yf = 255.0 * ((float)(ticks - ticks1) / 1000.0);
			if(yf < 0.0) yf = 0.0;
			if(yf > 255.0) yf = 255.0;
		}

		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, (int)yf);
		SDL_BLITVIDEO(videobuffer, NULL, video, NULL);
		SDL_SetAlpha(videobuffer, SDL_SRCALPHA, 255);

		sys_tickcalc();

		float add = 0.5 * fpsr;
		if(add > 1) add = 1;

		xofs = xofs + add;
		if(xofs >= 320.0) xofs -= 320.0;

		itemyloc += 0.75 * fpsr;

		while(itemyloc >= 16)
			itemyloc = itemyloc - 16;

		sys_pollevent();

		if(keypause < ticks) {
			keys = SDL_GetKeyState(NULL);

			if(event.type == SDL_KEYDOWN) {
				keypause = ticks + 150;

				if((keys[config.key_menu]) && mode == 1) break;
				else if(keys[config.key_up]) {
					cursel--;
					if(cursel < 0) cursel = (mode == 1 ? 4 : 3);
				} else if(keys[config.key_down]) {
					cursel++;
					if(cursel >= (mode == 1 ? 5 : 4)) cursel = 0;
				} else if(keys[config.key_attack] || keys[SDLK_RETURN]) {
					if(cursel == 0) {
						game_saveloadnew();
						ticks = SDL_GetTicks();
						keypause = ticks + 150;
						ticks1 = ticks;
					} else if(cursel == 1) {
						game_configmenu();
						ticks = SDL_GetTicks();
						keypause = ticks + 150;
						ticks1 = ticks;
					} else if(cursel == 2) {
						game_modsmenu();
						keypause = ticks + 150;
					} else if(cursel == 3) {
						exit(0);
					} else if(cursel == 4) break;
				}
			}
		}

		SDL_Flip(video);
		} while(1);

	itemticks = ticks + 210;

	if(menabled == 1 && config.music == 1) {
		Mix_HaltChannel(menuchannel);
		Mix_Resume(musicchannel);
		Mix_Volume(musicchannel, config.musicvol);
		pmenu = 0;
	}
}

void game_updanims()
{
	for(int i = 0; i <= lastobj; i++) {
		int nframes = objectinfo[i][0];
		int oanimspd = objectinfo[i][3];
		float frame = objectframe[i][0];
		int cframe = objectframe[i][1];
		// objectinfo[i][6] = 0; // ?? out of bounds

		if(nframes > 1) {
			frame = frame + oanimspd / 50 * fpsr;
			while(frame >= nframes)
				frame = frame - nframes;

			cframe = (int)frame; // truncate fractional part
			if(cframe > nframes) cframe = nframes - 1;
			if(cframe < 0) cframe = 0;

			objectframe[i][0] = frame;
			objectframe[i][1] = cframe;
		}
	}
}

void game_updatey()
{
	int yy, ff;

	for(int i = 0; i <= 2400; i++)
		ysort[i] = -1;

	ff = (int)(player.py * 10);
	if(ff < 0) ff = 0; // HACKFIX or ysort[yy] may go out of bounds
	player.ysort = ff;
	ysort[ff] = 0;

	firsty = 2400;
	lasty = 0;

	for(int i = 1; i <= lastnpc; i++) {
		yy = (int)(npcinfo[i].y * 10);

		do {
			if(ysort[yy] == -1 || yy == 2400) break;
			yy = yy + 1;
		} while(1);

		ysort[yy] = i;
		if(yy < firsty) firsty = yy;
		if(yy > lasty) lasty = yy;
	}
}

void game_updmusic()
{
    Mix_Chunk *iplaysound = NULL;

    if(menabled == 1 && config.music == 1) {

        iplaysound = mgardens;
        if(roomlock == 1) iplaysound = mboss;

        if(iplaysound == mboss && pboss) iplaysound = NULL;
        if(iplaysound == mgardens && pgardens) iplaysound = NULL;

        if(iplaysound != NULL) {
            Mix_HaltChannel(musicchannel);

            pboss = 0;
            pgardens = 0;
            ptown = 0;
            pacademy = 0;
            pcitadel = 0;

            if(iplaysound == mboss) pboss = 1;
            if(iplaysound == mgardens) pgardens = 1;

            if(pgardens == 1) {
                loopseta = 0;
                musicchannel = Mix_PlayChannel(-1, mgardens, 0);
            } else {
                musicchannel = Mix_PlayChannel(-1, iplaysound, -1);
            }
            Mix_Volume(musicchannel, config.musicvol);
        } else {
            if(pgardens == 1) {
                if(!Mix_Playing(musicchannel)) {
                    Mix_Chunk *tracks[4] = { mgardens, mgardens2, mgardens3, mgardens4 };
                    for(int attempt = 0; attempt < 4; attempt++) {
                        loopseta = (loopseta + 1) % 4;
                        if(tracks[loopseta]) {
                            musicchannel = Mix_PlayChannel(-1, tracks[loopseta], 0);
                            break;
                        }
                    }

                    Mix_Volume(musicchannel, config.musicvol);
                }
            }
        }
    }
}

void sys_initpaths()
{
	mods_activate();
}

static void game_loadtext(char **lines, char storage[][256], int count, const char *path)
{
	FILE *fp = mods_fopen(path, "r");
	if(!fp) {
		fprintf(stderr, "Missing %s in mod and griffonleg\n", path);
		exit(1);
	}
	for(int i = 0; i < count; i++) {
		storage[i][0] = 0;
		if(fgets(storage[i], 256, fp)) {
			storage[i][strcspn(storage[i], "\r\n")] = 0;
		}
		lines[i] = storage[i];
	}
	fclose(fp);
}

void sys_initialize()
{
	int result;

	sys_initpaths();
	game_loadtext(story, storytext, 48, "data/story.txt");
	game_loadtext(story2, story2text, 27, "data/story2.txt");

	// init char *floatstri[MAX_FLOATING_TEXTS]
	for(int i = 0; i < MAX_FLOATING_TEXTS; i++)
		floatstri[i] = malloc(64); // 64 bytes each string (should be enough)


	config_defaults(&config);
	config_load(&config);

	printf("SCR_WIDTH: %i\n", config.scr_width);
	printf("SCR_HEIGHT: %i\n", config.scr_height);
	printf("SCR_BITS: %i\n", config.scr_bpp);

	fullscreen = config.fullscreen | config.hwaccel | config.hwsurface;

	result = SDL_Init(SDL_INIT_EVERYTHING);
	if(result) {
		printf("Failed to init SDL\n");
		exit(1);
	}

	atexit(SDL_Quit);

	SDL_JoystickEventState(SDL_ENABLE);
	if(SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);

	video = game_setvideomode(config.scr_width, config.scr_height, config.scr_bpp, fullscreen); // SDL_FULLSCREEN
	if(!video) {
		printf("Failed to init Video\n");
		exit(1);
	}


	SDL_WM_SetCaption("The Griffon Legend", NULL);

	SDL_ShowCursor(SDL_DISABLE);

	videobuffer = SDL_CreateRGBSurface(config.hwsurface, 320, 240, config.scr_bpp, video->format->Rmask, video->format->Gmask, video->format->Bmask, video->format->Amask);
	videobuffer2 = SDL_CreateRGBSurface(config.hwsurface, 320, 240, config.scr_bpp, video->format->Rmask, video->format->Gmask, video->format->Bmask, video->format->Amask);
	videobuffer3 = SDL_CreateRGBSurface(config.hwsurface, 320, 240, config.scr_bpp, video->format->Rmask, video->format->Gmask, video->format->Bmask, video->format->Amask);
	mapbg = SDL_CreateRGBSurface(config.hwsurface, 320, 240, config.scr_bpp, video->format->Rmask, video->format->Gmask, video->format->Bmask, video->format->Amask);
	clipbg = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 240, config.scr_bpp, video->format->Rmask, video->format->Gmask, video->format->Bmask, video->format->Amask);
	clipbg2 = SDL_DisplayFormat(video);

	for(int i = 0; i <= 3; i++) {
		char name[128];

		sprintf(name, "art/map%i.bmp", i + 1);
		mapimg[i] = sys_loadimage(name);
		SDL_SetColorKey(mapimg[i], SDL_SRCCOLORKEY, SDL_MapRGB(mapimg[i]->format, 255, 0, 255));
	}

	cloudimg = sys_loadimage("art/clouds.bmp");
	SDL_SetColorKey(cloudimg, SDL_SRCCOLORKEY, SDL_MapRGB(cloudimg->format, 255, 0, 255));
	SDL_SetAlpha(cloudimg, 0 | SDL_SRCALPHA, 96);


	saveloadimg = sys_loadimage("art/saveloadnew.bmp");
	SDL_SetColorKey(saveloadimg, SDL_SRCCOLORKEY, SDL_MapRGB(saveloadimg->format, 255, 0, 255));
	SDL_SetAlpha(saveloadimg, 0 | SDL_SRCALPHA, 160);

	titleimg = sys_loadimage("art/titleb.bmp");
	titleimg2 = sys_loadimage("art/titlea.bmp");
	SDL_SetColorKey(titleimg2, SDL_SRCCOLORKEY, SDL_MapRGB(titleimg2->format, 255, 0, 255));
	//SDL_SetAlpha(titleimg2, 0 | SDL_SRCALPHA, 204);

	inventoryimg = sys_loadimage("art/inventory.bmp");
	SDL_SetColorKey(inventoryimg, SDL_SRCCOLORKEY, SDL_MapRGB(inventoryimg->format, 255, 0, 255));

	logosimg = sys_loadimage("art/logos.bmp");
	theendimg = sys_loadimage("art/theend.bmp");


	sys_LoadTiles();
	sys_LoadTriggers();
	sys_LoadObjectDB();
	sys_LoadAnims();
	sys_LoadFont();
	sys_LoadItemImgs();

	for(int i = 0; i <= 15; i++) {
		playerattackofs[0][i][0] = 0; // -1// -(i + 1)
		playerattackofs[0][i][1] = -sin(3.14159 * 2 * (i + 1) / 16) * 2 - 1;

		playerattackofs[1][i][0] = 0; // i + 1
		playerattackofs[1][i][1] = -sin(3.14159 * 2 * (i + 1) / 16) * 2 + 1;

		playerattackofs[2][i][0] = -1; // -(i + 1)
		playerattackofs[2][i][1] = -sin(3.14159 * 2 * (i + 1) / 16) * 2;

		playerattackofs[3][i][0] = 1; // i + 1
		playerattackofs[3][i][1] = -sin(3.14159 * 2 * (i + 1) / 16) * 2;
	}

	sys_setupAudio();
}

void sys_line(SDL_Surface *buffer, int x1, int y1, int x2, int y2, int col)
{
	unsigned int *temp;

	SDL_LockSurface(buffer);

	int xdif = x2 - x1;
	int ydif = y2 - y1;

	if(xdif == 0) {
		for(int y = y1; y <= y2; y++) {
			temp = buffer->pixels + y * buffer->pitch + x1 * buffer->format->BytesPerPixel;
			*temp = col;
		}
	}

	if(ydif == 0) {
		for(int x = x1; x <= x2; x++) {
			temp = buffer->pixels + y1 * buffer->pitch + x * buffer->format->BytesPerPixel;
			*temp = col;
		}
	}

	SDL_UnlockSurface(buffer);
}

void sys_LoadAnims()
{
	spellimg = sys_loadimage("art/spells.bmp");
	SDL_SetColorKey(spellimg, SDL_SRCCOLORKEY, SDL_MapRGB(spellimg->format, 255, 0, 255));

	anims[0] = sys_loadimage("art/anims0.bmp");
	SDL_SetColorKey(anims[0], SDL_SRCCOLORKEY, SDL_MapRGB(anims[0]->format, 255, 0, 255));

	animsa[0] = sys_loadimage("art/anims0a.bmp");
	SDL_SetColorKey(animsa[0], SDL_SRCCOLORKEY, SDL_MapRGB(animsa[0]->format, 255, 0, 255));

	anims[13] = sys_loadimage("art/anims0x.bmp");
	SDL_SetColorKey(anims[13], SDL_SRCCOLORKEY, SDL_MapRGB(anims[13]->format, 255, 0, 255));

	animsa[13] = sys_loadimage("art/anims0xa.bmp");
	SDL_SetColorKey(animsa[13], SDL_SRCCOLORKEY, SDL_MapRGB(animsa[13]->format, 255, 0, 255));

	anims[1] = sys_loadimage("art/anims1.bmp");
	SDL_SetColorKey(anims[1], SDL_SRCCOLORKEY, SDL_MapRGB(anims[1]->format, 255, 0, 255));

	animsa[1] = sys_loadimage("art/anims1a.bmp");
	SDL_SetColorKey(animsa[1], SDL_SRCCOLORKEY, SDL_MapRGB(animsa[1]->format, 255, 0, 255));

	anims[2] = sys_loadimage("art/anims2.bmp");
	SDL_SetColorKey(anims[2], SDL_SRCCOLORKEY, SDL_MapRGB(anims[2]->format, 255, 0, 255));

	// huge
	animset2[0].xofs = 8;
	animset2[0].yofs = 7;
	animset2[0].x = 123;
	animset2[0].y = 0;
	animset2[0].w = 18;
	animset2[0].h = 16;
	// big
	animset2[1].xofs = 7;
	animset2[1].yofs = 7;
	animset2[1].x = 107;
	animset2[1].y = 0;
	animset2[1].w = 16;
	animset2[1].h = 14;
	// med
	animset2[2].xofs = 6;
	animset2[2].yofs = 6;
	animset2[2].x = 93;
	animset2[2].y = 0;
	animset2[2].w = 14;
	animset2[2].h = 13;
	// small
	animset2[3].xofs = 4;
	animset2[3].yofs = 4;
	animset2[3].x = 83;
	animset2[3].y = 0;
	animset2[3].w = 10;
	animset2[3].h = 10;
	// wing
	animset2[4].xofs = 4;
	animset2[4].yofs = 20;
	animset2[4].x = 42;
	animset2[4].y = 0;
	animset2[4].w = 41;
	animset2[4].h = 33;
	// head
	animset2[5].xofs = 20;
	animset2[5].yofs = 18;
	animset2[5].x = 0;
	animset2[5].y = 0;
	animset2[5].w = 42;
	animset2[5].h = 36;

	anims[9] = sys_loadimage("art/anims9.bmp");
	SDL_SetColorKey(anims[9], SDL_SRCCOLORKEY, SDL_MapRGB(anims[9]->format, 255, 0, 255));

	// huge
	animset9[0].xofs = 8;
	animset9[0].yofs = 7;
	animset9[0].x = 154;
	animset9[0].y = 0;
	animset9[0].w = 18;
	animset9[0].h = 16;
	// big
	animset9[1].xofs = 7;
	animset9[1].yofs = 7;
	animset9[1].x = 138;
	animset9[1].y = 0;
	animset9[1].w = 16;
	animset9[1].h = 14;
	// med
	animset9[2].xofs = 6;
	animset9[2].yofs = 6;
	animset9[2].x = 93 + 31;
	animset9[2].y = 0;
	animset9[2].w = 14;
	animset9[2].h = 13;
	// small
	animset9[3].xofs = 4;
	animset9[3].yofs = 4;
	animset9[3].x = 83 + 31;
	animset9[3].y = 0;
	animset9[3].w = 10;
	animset9[3].h = 10;
	// wing
	animset9[4].xofs = 36;
	animset9[4].yofs = 20;
	animset9[4].x = 42;
	animset9[4].y = 0;
	animset9[4].w = 72;
	animset9[4].h = 33;
	// head
	animset9[5].xofs = 20;
	animset9[5].yofs = 18;
	animset9[5].x = 0;
	animset9[5].y = 0;
	animset9[5].w = 42;
	animset9[5].h = 36;

	anims[3] = sys_loadimage("art/anims3.bmp");
	SDL_SetColorKey(anims[3], SDL_SRCCOLORKEY, SDL_MapRGB(anims[3]->format, 255, 0, 255));

	anims[4] = sys_loadimage("art/anims4.bmp");
	SDL_SetColorKey(anims[4], SDL_SRCCOLORKEY, SDL_MapRGB(anims[4]->format, 255, 0, 255));

	anims[5] = sys_loadimage("art/anims5.bmp");
	SDL_SetColorKey(anims[5], SDL_SRCCOLORKEY, SDL_MapRGB(anims[5]->format, 255, 0, 255));

	anims[6] = sys_loadimage("art/anims6.bmp");
	SDL_SetColorKey(anims[6], SDL_SRCCOLORKEY, SDL_MapRGB(anims[6]->format, 255, 0, 255));

	anims[7] = sys_loadimage("art/anims7.bmp");
	SDL_SetColorKey(anims[7], SDL_SRCCOLORKEY, SDL_MapRGB(anims[7]->format, 255, 0, 255));

	anims[8] = sys_loadimage("art/anims8.bmp");
	SDL_SetColorKey(anims[8], SDL_SRCCOLORKEY, SDL_MapRGB(anims[8]->format, 255, 0, 255));

	anims[10] = sys_loadimage("art/anims10.bmp");
	SDL_SetColorKey(anims[10], SDL_SRCCOLORKEY, SDL_MapRGB(anims[10]->format, 255, 0, 255));

	animsa[10] = sys_loadimage("art/anims10a.bmp");
	SDL_SetColorKey(animsa[10], SDL_SRCCOLORKEY, SDL_MapRGB(animsa[10]->format, 255, 0, 255));

	anims[11] = sys_loadimage("art/anims11.bmp");
	SDL_SetColorKey(anims[11], SDL_SRCCOLORKEY, SDL_MapRGB(anims[11]->format, 255, 0, 255));

	animsa[11] = sys_loadimage("art/anims11a.bmp");
	SDL_SetColorKey(animsa[11], SDL_SRCCOLORKEY, SDL_MapRGB(animsa[11]->format, 255, 0, 255));

	anims[12] = sys_loadimage("art/anims12.bmp");
	SDL_SetColorKey(anims[12], SDL_SRCCOLORKEY, SDL_MapRGB(anims[12]->format, 255, 0, 255));
}

void sys_LoadItemImgs()
{
	SDL_Surface *temp;

	temp = sys_loadimage("art/icons.bmp");

	for(int i = 0; i <= 20; i++) {
		itemimg[i] = SDL_CreateRGBSurface(SDL_SWSURFACE, 16, 16, config.scr_bpp, video->format->Rmask, video->format->Gmask, video->format->Bmask, video->format->Amask);
		SDL_SetColorKey(itemimg[i], SDL_SRCCOLORKEY, SDL_MapRGB(itemimg[i]->format, 255, 0, 255));

		rcSrc.x = i * 16;
		rcSrc.y = 0;
		rcSrc.w = 16;
		rcSrc.h = 16;

		SDL_BlitSurface(temp, &rcSrc, itemimg[i], NULL);
	}

	SDL_FreeSurface(temp);
}

void sys_LoadFont()
{
	SDL_Surface *font;

	font = sys_loadimage("art/font.bmp");

	for(int i = 32; i <= 255; i++)
		for(int f = 0; f <= 4; f++) {
			int i2 = i - 32;

			fontchr[i2][f] = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, config.scr_bpp, video->format->Rmask, video->format->Gmask, video->format->Bmask, video->format->Amask);
			SDL_SetColorKey(fontchr[i2][f], SDL_SRCCOLORKEY, SDL_MapRGB(fontchr[i2][f]->format, 255, 0, 255));

			int col = i2 % 40;

			int row = (i2 - col) / 40;

			rcSrc.x = col * 8;
			rcSrc.y = row * 8 + f * 48;
			rcSrc.w = 8;
			rcSrc.h = 8;

			rcDest.x = 0;
			rcDest.y = 0;
			SDL_BlitSurface(font, &rcSrc, fontchr[i2][f], &rcDest);
		}

	SDL_FreeSurface(font);
}

void sys_LoadTiles()
{
	tiles[0] = sys_loadimage("art/tx.bmp");
	tiles[1] = sys_loadimage("art/tx1.bmp");
	tiles[2] = sys_loadimage("art/tx2.bmp");
	tiles[3] = sys_loadimage("art/tx3.bmp");

	for(int i = 0; i <= 3; i++)
		SDL_SetColorKey(tiles[i], SDL_SRCCOLORKEY, SDL_MapRGB(tiles[i]->format, 255, 0, 255));

	windowimg = sys_loadimage("art/window.bmp");
	SDL_SetColorKey(windowimg, SDL_SRCCOLORKEY, SDL_MapRGB(windowimg->format, 255, 0, 255));
}

void sys_LoadTriggers()
{
	FILE *fp;
	char line[512];

	fp = mods_fopen("data/triggers.dat", "r");
	if(!fp) {
		fprintf(stderr, "Missing data/triggers.dat in mod and griffonleg\n");
		exit(1);
	}

	for(int i = 0; i <= 9999; i++)
		for(int a = 0; a <= 8; a++) {
			fgets(line, sizeof(line), fp);
			sscanf(line, "%i", &triggers[i][a]);
		}

	fclose(fp);
}

void sys_LoadObjectDB()
{
	FILE *fp;
	char line[512];

	fp = mods_fopen("objectdb.dat", "r");
	if(!fp) {
		fprintf(stderr, "Missing objectdb.dat in mod and griffonleg\n");
		exit(1);
	}

	for(int a = 0; a <= 32; a++) {
		for(int b = 0; b <= 5; b++) {
			fgets(line, sizeof(line), fp);
			sscanf(line, "%i", &objectinfo[a][b]);
		}

		for(int b = 0; b <= 8; b++) {
			for(int c = 0; c <= 2; c++) {
				for(int d = 0; d <= 2; d++) {
					for(int e = 0; e <= 1; e++) {
						fgets(line, sizeof(line), fp);
						sscanf(line, "%i", &objecttile[a][b][c][d][e]);
					}
				}
			}
		}
	}

	fclose(fp);
}

void sys_print(SDL_Surface *buffer, char *stri, int xloc, int yloc, int col)
{
	int l = strlen(stri);

	for(int i = 0; i < l; i++) {
		rcDest.x = xloc + i * 8;
		rcDest.y = yloc;

		SDL_BlitSurface(fontchr[stri[i] - 32][col], NULL, buffer, &rcDest);
	}
}

void sys_progress(int w, int wm)
{
	long ccc;

	ccc = SDL_MapRGB(videobuffer->format, 0, 255, 0);

	rcDest.w = w * 74 / wm;
	SDL_FillRect(videobuffer, &rcDest, ccc);
	SDL_BLITVIDEO(videobuffer, NULL, video, NULL);
	SDL_Flip(video);
	SDL_PumpEvents();
}

void sys_setupAudio()
{
	SDL_Surface *loadimg;

	menabled = 1;

	if((Mix_Init(MIX_INIT_OGG) & MIX_INIT_OGG) != MIX_INIT_OGG) {
		fprintf(stderr, "OGG support unavailable; continuing without audio\n");
		menabled = 0;
		return;
	}

	if(Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
		fprintf(stderr, "Audio unavailable: %s; continuing without audio\n", Mix_GetError());
		menabled = 0;
		return;
	}

	atexit(Mix_Quit);
	atexit(Mix_CloseAudio);

	char *stri = "Loading...";
	sys_print(videobuffer, stri, 160 - 4 * strlen(stri), 116, 0);

	loadimg = sys_loadimage("art/load.bmp");
	SDL_SetColorKey(loadimg, SDL_SRCCOLORKEY, SDL_MapRGB(loadimg->format, 255, 0, 255));

	rcSrc.x = 0;
	rcSrc.y = 0;
	rcSrc.w = 88;
	rcSrc.h = 32;

	rcDest.x = 160 - 44;
	rcDest.y = 116 + 12;

	SDL_SetAlpha(loadimg, 0 | SDL_SRCALPHA, 160); // 128
	SDL_BlitSurface(loadimg, &rcSrc, videobuffer, &rcDest);
	SDL_SetAlpha(loadimg, 0 | SDL_SRCALPHA, 255);

	SDL_BLITVIDEO(videobuffer, NULL, video, NULL);
	SDL_Flip(video);

	rcDest.x = 160 - 44 + 7;
	rcDest.y = 116 + 12 + 12;
	rcDest.h = 8;

	if(menabled == 1) {
		mboss = sys_loadchunk("music/boss.ogg");
		sys_progress(1, 21);
		mgardens = sys_loadchunk("music/gardens.ogg");
		sys_progress(2, 21);
		mgardens2 = sys_loadmodchunk("music/gardens2.ogg");
		sys_progress(3, 21);
		mgardens3 = sys_loadmodchunk("music/gardens3.ogg");
		sys_progress(4, 21);
		mgardens4 = sys_loadmodchunk("music/gardens4.ogg");
		sys_progress(5, 21);
		mendofgame = sys_loadchunk("music/endofgame.ogg");
		sys_progress(6, 21);
		mmenu = sys_loadchunk("music/menu.ogg");
		sys_progress(7, 21);

		sfx[0] = sys_loadchunk("sfx/bite.ogg");
		sys_progress(8, 21);
		sfx[1] = sys_loadchunk("sfx/crystal.ogg");
		sys_progress(9, 21);
		sfx[2] = sys_loadchunk("sfx/door.ogg");
		sys_progress(10, 21);
		sfx[3] = sys_loadchunk("sfx/enemyhit.ogg");
		sys_progress(11, 21);
		sfx[4] = sys_loadchunk("sfx/ice.ogg");
		sys_progress(12, 21);
		sfx[5] = sys_loadchunk("sfx/lever.ogg");
		sys_progress(13, 21);
		sfx[6] = sys_loadchunk("sfx/lightning.ogg");
		sys_progress(14, 21);
		sfx[7] = sys_loadchunk("sfx/metalhit.ogg");
		sys_progress(15, 21);
		sfx[8] = sys_loadchunk("sfx/powerup.ogg");
		sys_progress(16, 21);
		sfx[9] = sys_loadchunk("sfx/rocks.ogg");
		sys_progress(17, 21);
		sfx[10] = sys_loadchunk("sfx/swordhit.ogg");
		sys_progress(18, 21);
		sfx[11] = sys_loadchunk("sfx/throw.ogg");
		sys_progress(19, 21);
		sfx[12] = sys_loadchunk("sfx/chest.ogg");
		sys_progress(20, 21);
		sfx[13] = sys_loadchunk("sfx/fire.ogg");
		sys_progress(21, 21);
		sfx[14] = sys_loadchunk("sfx/beep.ogg");
	}

	SDL_FreeSurface(loadimg);
}

void sys_update()
{
	unsigned int *temp, bgc;
	int pa, sx, sy;
	float opx, opy, spd;
	static int nextgamesecond;

	SDL_Flip(video);
	SDL_PumpEvents();

	sys_tickcalc();
	if(nextgamesecond == 0) nextgamesecond = ticks + 1000;
	if(ticks >= nextgamesecond) {
		secsingame++;
		nextgamesecond = ticks + 1000;
	}

	SDL_LockSurface(clipbg);

	if(attacking == 1) {
		player.attackframe += player.attackspd * fpsr;
		if(player.attackframe >= 16) {
			attacking = 0;
			player.attackframe = 0;
			player.walkframe = 0;
		}

		pa = (int)(player.attackframe);

		for(int i = 0; i <= pa; i++) {
			if(playerattackofs[player.walkdir][i][2] == 0) {
				playerattackofs[player.walkdir][i][2] = 1;

				opx = player.px;
				opy = player.py;

				player.px = player.px + playerattackofs[player.walkdir][i][0];
				player.py = player.py + playerattackofs[player.walkdir][i][1];

				sx = (int)(player.px / 2 + 6);
				sy = (int)(player.py / 2 + 10);
				temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
				bgc = *temp;
				if(!cheat_noclip && bgc > 0) {
					player.px = opx;
					player.py = opy;
				}
			}
		}

		player.opx = player.px;
		player.opy = player.py;

		game_checkhit();
	}

	for(int i = 0; i < MAX_FLOATING_TEXTS; i++) {
		if(floattext[i][0] > 0) {
			spd = 0.5 * fpsr;
			floattext[i][0] = floattext[i][0] - spd;
			floattext[i][2] = floattext[i][2] - spd;
			if(floattext[i][0] < 0) floattext[i][0] = 0;
		}

		if(floaticon[i][0] > 0) {
			spd = 0.5 * fpsr;
			floaticon[i][0] = floaticon[i][0] - spd;
			floaticon[i][2] = floaticon[i][2] - spd;
			if(floaticon[i][0] < 0) floaticon[i][0] = 0;
		}
	}

	if(player.level == player.maxlevel) player.exp = 0;

	if(player.exp >= player.nextlevel) {
		player.level = player.level + 1;
		game_addFloatText("LEVEL UP!", player.px + 16 - 36, player.py + 16, 3);
		player.exp = player.exp - player.nextlevel;
		player.nextlevel = player.nextlevel * 3 / 2; // 1.5
		player.maxhp = player.maxhp + player.level * 3;
		if(player.maxhp > 999) player.maxhp = 999;
		player.hp = player.maxhp;

		player.sworddamage = player.level * 3 / 4;
		player.spelldamage = player.level * 13 / 10;

		game_playsound(sndpowerup);
	}

	SDL_UnlockSurface(clipbg);
	SDL_BlitSurface(clipbg2, NULL, clipbg, NULL);

	SDL_Rect rc;

	rc.x = player.px - 2;
	rc.y = player.py - 2;
	rc.w = 5;
	rc.h = 5;

	SDL_FillRect(clipbg, &rc, 1000);

	if(forcepause == 0) {
		for(int i = 0; i < 5; i++) {
			if(player.foundspell[i] == 1) player.spellcharge[i] += 1 * player.level * 0.01 * fpsr;
			if(player.spellcharge[i] > 100) player.spellcharge[i] = 100;
		}

		if(player.foundspell[0]) {
			player.spellstrength += 3 * player.level * .01 * fpsr;
		}

		player.attackstrength += (30 + 3 * (float)player.level) / 50 * fpsr;
	}

	if(player.attackstrength > 100) player.attackstrength = 100;

	if(player.spellstrength > 100) player.spellstrength = 100;

	itemyloc += 0.75 * fpsr;
	while(itemyloc >= 16) itemyloc -= 16;

	if(player.hp <= 0) game_theend();

	if(roomlock == 1) {
		roomlock = 0;
		for(int i = 1; i <= lastnpc; i++)
			if(npcinfo[i].hp > 0) roomlock = 1;
	}

	clouddeg += 0.1 * fpsr;
	while(clouddeg >= 360) clouddeg = clouddeg - 360;

	player.hpflash = player.hpflash + 0.1 * fpsr;
	if(player.hpflash >=2) {
		player.hpflash = 0;
		player.hpflashb = player.hpflashb + 1;
		if(player.hpflashb == 2) player.hpflashb = 0;
		if(!cheat_god && menabled == 1 && config.effects == 1 && player.hpflashb == 0 && player.hp < player.maxhp / 4) {
			int snd = Mix_PlayChannel(-1, sfx[sndbeep], 0);
			Mix_Volume(snd, config.effectsvol);
		}
	}

	// cloudson = 0

	if(itemselon == 1) player.itemselshade = player.itemselshade + 2 * fpsr;
	if(player.itemselshade > 24) player.itemselshade = 24;

	for(int i = 0; i <= 4; i++)
		if(player.inventory[i] > 9) player.inventory[i] = 9;

	if(forcepause == 0) game_time += tickspassed;
}
