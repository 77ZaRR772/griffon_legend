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

#ifndef _GRIFFON_H_
#define _GRIFFON_H_

#include <SDL/SDL.h>

#define MAXNPC		32
#define MAX_FLOATING_TEXTS    32
#define MAXSPELL	32

// spells
#define ice		0
#define steel		1
#define wood		2
#define rock		3
#define fire		4

// inventory items
#define INV_FLASK	0
#define INV_DOUBLEFLASK	1
#define INV_SHOCK	2
#define INV_NORMALKEY	3
#define INV_MASTERKEY	4

#define sndbite		0
#define sndcrystal	1
#define snddoor		2
#define sndenemyhit	3
#define sndice		4
#define sndlever	5
#define sndlightning	6
#define sndmetalhit	7
#define sndpowerup	8
#define sndrocks	9
#define sndswordhit	10
#define sndthrow	11
#define sndchest	12
#define sndfire		13
#define sndbeep		14

typedef struct {
	float	px;
	float	py;
	float	opx;
	float	opy;
	int	walkdir;
	float	walkframe;
	float	walkspd;
	float	attackframe;
	float	attackspd;

	int	hp;
	int	maxhp;
	float	hpflash;
	int	hpflashb;
	int	level;
	int	maxlevel;
	int	sword;
	int	shield;
	int	armour;
	int	foundspell[5];
	float	spellcharge[5];
	int	inventory[5];
	float	attackstrength;
	float	spellstrength;
	int	spelldamage;
	int	sworddamage;

	int	exp;
	int	nextlevel;

	int	pause;

	float	itemselshade;
	int	ysort;

} PLAYERTYPE;

typedef struct {
	float	x;
	float	y;
	int	parentID;
	int	isbase;
	int	sprite;
	int	bonelength;	// the 'bone' that connects the body sections
} BODYSECTIONTYPE;

typedef struct {
	float	x;
	float	y;
	int	spriteset;
	int	x1;		// patrol area
	int	y1;
	int	x2;
	int	y2;
	int	attitude;
	int	hp;
	float	spelldamagecarry;

	int	maxhp;
	int	item1;
	int	item2;
	int	item3;
	int	script;
	float	frame;
	float	frame2;		// end boss specific
	int	cframe;
	int	onmap;		// is this npc set to be genned in the mapfile

	int	ticks;
	int	pause;
	int	shake;

	int	movementmode;
	int	walkdir;
	float	walkspd;
	int	movingdir;
	int	moving;

	int	attacking;
	float	attackframe;
	int	cattackframe;
	float	attackspd;
	int	attackdelay;
	int	attacknext;
	int	specialnext;
	int	attackattempt;

	int	spelldamage;
	int	attackdamage;


	// one wing and firehydra specific
	BODYSECTIONTYPE bodysection[31];
	float	swayangle;
	float	swayspd;
	float	headtargetx[4];
	float	headtargety[4];
	int	castpause;

	// firehydra specific
	int	attacknext2[4];
	int	attacking2[4];
	int	attackframe2[4];

	// dragon2 specific
	float	floating;

	//fixes for miss! text and attacking sound spam
	int lastmisstick;
	int aggro;

} NPCTYPE;

typedef struct {
	int	spellnum;
	float	homex;
	float	homey;
	float	enemyx;
	float	enemyy;

	float	frame;

	int	damagewho;	// 0 = npc, 1 = player

	// for earthslide
	float	rocky[9];
	int	rockimg[9];
	int	rockdeflect[9];

	float	strength;

	// fire
	int	legalive[5];

	// spell 6 specific
	float	fireballs[7][4];	// x,y,targetx, targety
	int	nfballs;
	int	ballon[7];

	int	npc;
	int	targetnpc;
	int	hitnext[MAXNPC];
} SPELLTYPE;

typedef struct {
	int	x;	// xyloc on spriteimageset
	int	y;
	int	xofs;	// the actual place to paste the sprite in reference to the bodypart loc on screen
	int	yofs;
	int	w;	// w/h of the sprite in the imageset
	int	h;
} ANIMSET2TYPE;

extern PLAYERTYPE player;
extern SDL_Event event;
extern int curmap;
extern int scriptflag[100][10];
extern int objmapf[1000][21][15];
extern int roomlocks[201];
extern int secsingame, secstart;

void game_addFloatIcon(int ico, float xloc, float yloc);
void game_addFloatText(char *stri, float xloc, float yloc, int col);
void game_attack(void);
void game_castspell(int spellnum, float homex, float homey, float enemyx, float enemyy, int damagewho);
void game_checkhit(void);
void game_checkinputs(void);
void game_configmenu(void);
void game_damagenpc(int npcnum, int damage, int spell);
void game_damageplayer(int damage);
void game_drawanims(int Layer);
void game_draweventtext(void);
void game_drawhud(void);
void game_drawnpcs(int mode);
void game_drawover(int modx, int mody);
void game_drawplayer(void);
void game_drawview(void);
void game_endofgame(void);
void game_eventtext(char *stri);
void game_fillrect(SDL_Surface *surface, int x, int y, int w, int h, int color);
void game_handlewalking(void);
void game_loadmap(int mapnum);
int game_readmap(const char *name, int tempmap[320][200]);
void game_setpause(int paused);
void game_main(void);
void game_newgame(void);
void game_playgame(void);
void game_processtrigger(int trignum);
void game_saveloadnew(void);
void game_showlogos(void);
void game_swash(void);
void game_theend(void);
void game_title(int mode);
void game_updanims(void);
void game_updatey(void);
void game_updmusic(void);
void game_updnpcs(void);
void game_updspells(void);
void game_updspellsunder(void);

void sdl_blitscale(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
void sys_initialize(void);
int sys_pollevent(void);
void sys_line(SDL_Surface *buffer, int x1, int y1, int x2, int y2, int col);
void sys_LoadAnims(void);
void sys_LoadFont(void);
void sys_LoadItemImgs(void);
void sys_LoadTiles(void);
void sys_LoadTriggers(void);
void sys_print(SDL_Surface *buffer, char *stri, int xloc, int yloc, int col);
void sys_progress(int w, int wm);
void sys_LoadObjectDB(void);
void sys_setupAudio(void);
void sys_tickcalc(void);
void sys_update(void);

#endif // _GRIFFON_H_
