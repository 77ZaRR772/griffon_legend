#include <stdlib.h>
#include <SDL/SDL.h>

#include "griffon.h"
#include "items.h"

extern SDL_Surface *clipbg, *clipbg2;
extern SDL_Rect rcDest;
extern NPCTYPE npcinfo[MAXNPC];
extern int lastnpc, curmap, ticks, itemticks, attacking;
extern int movingup, movingdown, movingleft, movingright;
extern int objectinfo[33][6], objmap[21][15];
extern float playerattackofs[4][16][3];

static float items_rnd(void)
{
	return (float)rand() * (1.0 / ((float)RAND_MAX + 1.0));
}
#define RND() items_rnd()

void game_attack()
{
	float npx, npy;

	npx = player.px + 12;
	npy = player.py + 20;

	int lx = (int)npx / 16;
	int ly = (int)npy / 16;

	// if facing up
	if(player.walkdir == 0) {
		if(ly > 0) {
			int o2 = 0; // ??
			int o = objmap[lx][ly - 1];
			if(ly > 1 && curmap == 58) o2 = objmap[lx][ly - 2];
			if(ly > 1 && curmap == 54) o2 = objmap[lx][ly - 2];

			// cst
			if((objectinfo[o][4] == 1 && (o == 0 || o > 4)) || (objectinfo[o2][4] == 0 && o2 == 10)) {
				if(o2 == 10) o = 10;

				int oscript = objectinfo[o][5];
				if(oscript == 0 && player.inventory[INV_FLASK] < 9) {
					player.inventory[INV_FLASK]++;
					game_addFloatIcon(6, lx * 16, (ly - 1) * 16);

					objmapf[curmap][lx][ly - 1] = 1;

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;

					game_eventtext("Found Flask!");
					itemticks = ticks + 215;
					return;
				}

				if(oscript == 0 && player.inventory[INV_FLASK] == 9) {
					game_playsound(sndchest);

					game_eventtext("Cannot Carry any more Flasks!");
					itemticks = ticks + 215;
					return;
				}

				if(oscript == 2) {
					player.inventory[INV_MASTERKEY]++;

					game_addFloatIcon(14, lx * 16, (ly - 1) * 16);

					itemticks = ticks + 215;

					if(curmap == 34) scriptflag[2][0] = 2;
					if(curmap == 62) scriptflag[8][0] = 2;
					if(curmap == 81) scriptflag[13][0] = 2;

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;
					game_eventtext("Found the Temple Key!");
					return;
				}

				if(oscript == 3) {
					player.foundspell[0] = 1;
					player.spellcharge[0] = 0;

					game_addFloatIcon(7, lx * 16, (ly - 1) * 16);

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;

					game_eventtext("Found the Infinite Crystal!");
					itemticks = ticks + 215;
					return;
				}

				if(oscript == 4 && player.shield == 1) {
					player.shield = 2;

					game_addFloatIcon(4, lx * 16, (ly - 1) * 16);

					itemticks = ticks + 215;

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;

					game_eventtext("Found the Obsidian Shield!");
					objmapf[4][1][2] = 1;
					return;
				}

				if(oscript == 5 && player.sword == 1) {
					player.sword = 2;

					game_addFloatIcon(3, lx * 16, (ly - 1) * 16);

					itemticks = ticks + 215;

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;
					game_eventtext("Found the Fidelis Sword!");
					return;
				}

				if(oscript == 6) {
					if(player.inventory[INV_NORMALKEY] < 9) {
						player.inventory[INV_NORMALKEY]++;

						for(int s = 20; s <= 23; s++) {
							if(scriptflag[s][0] == 1) {
								scriptflag[s][0] = 2;
							}
						}

						game_playsound(sndpowerup);

						objmapf[curmap][lx][ly - 1] = 1;

						if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;

						game_eventtext("Found Key");
						game_addFloatIcon(16, lx * 16, (ly - 1) * 16);
					} else {
						game_playsound(sndchest);

						game_eventtext("Cannot Carry Any More Keys");
					}
				}

				if(oscript == 7 && player.inventory[INV_DOUBLEFLASK] < 9) {
					player.inventory[INV_DOUBLEFLASK]++;
					game_addFloatIcon(12, lx * 16, (ly - 1) * 16);

					objmapf[curmap][lx][ly - 1] = 1;

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;

					game_eventtext("Found Mega Flask!");
					itemticks = ticks + 215;
					return;
				}

				if(oscript == 7 && player.inventory[INV_DOUBLEFLASK] == 9) {
					game_playsound(sndchest);

					game_eventtext("Cannot Carry any more Mega Flasks!");
					itemticks = ticks + 215;
					return;
				}

				if(oscript == 10 && player.inventory[INV_DOUBLEFLASK] < 9) {
					player.inventory[INV_DOUBLEFLASK]++;
					game_addFloatIcon(12, lx * 16, (ly - 1) * 16);

					objmapf[curmap][lx][ly - 1] = 1;

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;

					game_eventtext("Found Mega Flask!");
					itemticks = ticks + 215;
					return;
				}

				if(oscript == 10 && player.inventory[INV_DOUBLEFLASK] == 9) {
					game_playsound(sndchest);

					game_eventtext("Cannot Carry any more Mega Flasks!");
					itemticks = ticks + 215;
					return;
				}

				if(oscript == 11 && player.inventory[INV_SHOCK] < 9) {
					player.inventory[INV_SHOCK]++;
					game_addFloatIcon(17, lx * 16, (ly - 1) * 16);

					objmapf[curmap][lx][ly - 1] = 1;

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;

					game_eventtext("Found Lightning Bomb!");
					itemticks = ticks + 215;
					return;
				}

				if(oscript == 11 && player.inventory[INV_SHOCK] == 9) {
					game_playsound(sndchest);

					game_eventtext("Cannot Carry any more Lightning Bombs!");
					itemticks = ticks + 215;
					return;
				}

				if(oscript == 12 && player.armour == 1) {
					player.armour = 2;

					game_addFloatIcon(5, lx * 16, (ly - 1) * 16);

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;

					game_eventtext("Found the Fidelis Mail!");
					itemticks = ticks + 215;
					return;
				}

				if(oscript == 60) {
					if(curmap == 58 && scriptflag[60][0] == 0) {
						scriptflag[60][0] = 1;

						game_playsound(sndlever);

					} else if(curmap == 58 && scriptflag[60][0] > 0) {
						game_playsound(snddoor);

						game_eventtext("It's stuck!");
					}

					if(curmap == 54 && scriptflag[60][0] == 1) {
						game_playsound(sndlever);

						scriptflag[60][0] = 2;
					} else if(curmap == 54 && scriptflag[60][0] > 1) {
						game_playsound(snddoor);

						game_eventtext("It's stuck!");
					}

				}

				if(oscript == 15 && player.sword < 3) {
					player.sword = 3;

					game_addFloatIcon(18, lx * 16, (ly - 1) * 16);

					itemticks = ticks + 215;

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;
					game_eventtext("Found the Blood Sword!");
					objmapf[4][1][2] = 1;
					return;
				}

				if(oscript == 16 && player.shield < 3) {
					player.shield = 3;
					game_addFloatIcon(19, lx * 16, (ly - 1) * 16);
					itemticks = ticks + 215;

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;
					game_eventtext("Found the Entropy Shield!");
					objmapf[4][1][2] = 1;
					return;
				}

				if(oscript == 17 && player.armour < 3) {
					player.armour = 3;
					game_addFloatIcon(20, lx * 16, (ly - 1) * 16);
					itemticks = ticks + 215;

					game_playsound(sndpowerup);

					if(objectinfo[o][4] == 1) objmap[lx][ly - 1] = 3;
					game_eventtext("Found the Rubyscale Armour!");
					objmapf[4][1][2] = 1;
					return;
				}

			}
		}
	}

	attacking = 1;
	player.attackframe = 0;
	movingup = 0;
	movingdown = 0;
	movingleft = 0;
	movingright = 0;

	for(int i = 0; i <= 15; i++) {
		for(int a = 0; a <= 3; a++) {
			playerattackofs[a][i][2] = 0;
		}
	}
}

int items_allenemiesdead(void) {
	for(int i = 1; i <= lastnpc; i++) {
		if(npcinfo[i].hp > 0) return 0;
	}
	return 1;
}

void items_drop(int npcnum, int obj_id, int mult) {
	int ff = (int)(RND() * player.level * mult);
	if(ff == 0) {
		int lx = (int)(npcinfo[npcnum].x + 12) / 16;
		int ly = (int)(npcinfo[npcnum].y + 20) / 16;
		if(objmap[lx][ly] == -1) objmap[lx][ly] = obj_id;
	}
}

void items_spawnchest(int cx, int cy, int obj_id, int flag_idx) {
	if(!items_allenemiesdead()) return;

	objmap[cx][cy] = obj_id;

	rcDest.x = cx * 8;
	rcDest.y = cy * 8;
	rcDest.w = 8;
	rcDest.h = 8;

	int lx = (int)(player.px + 12) / 16;
	int ly = (int)(player.py + 20) / 16;

	if(lx == cx && ly == cy) player.py = player.py + 16;

	SDL_FillRect(clipbg2, &rcDest, SDL_MapRGB(clipbg->format, 255,255,255));
	if (flag_idx >= 0) scriptflag[flag_idx][0] = 1;
}

void items_placeobject(int cx, int cy, int object)
{
	objmap[cx][cy] = object;
	rcDest.x = cx * 8;
	rcDest.y = cy * 8;
	rcDest.w = 8;
	rcDest.h = 8;

	int lx = (int)(player.px + 12) / 16;
	int ly = (int)(player.py + 20) / 16;
	if(lx == cx && ly == cy) player.py += 16;
	SDL_FillRect(clipbg, &rcDest, SDL_MapRGB(clipbg->format, 255, 255, 255));
}

