#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>

#include "griffon.h"
#include "config.h"
#include "debug.h"
#include "render.h"

extern SDL_Surface *video, *videobuffer, *videobuffer2, *mapbg, *clipbg;
extern SDL_Surface *spellimg, *cloudimg, *inventoryimg, *windowimg;
extern SDL_Surface *anims[100], *animsa[100], *itemimg[21], *mapimg[4], *tiles[4];
extern SDL_Rect rcSrc, rcDest;
extern NPCTYPE npcinfo[MAXNPC];
extern int lastnpc, firsty, lasty, ysort[2401], curmap, game_time;
extern int tileinfo[3][40][24][3], objmap[21][15], objectinfo[33][6];
extern int objecttile[33][9][3][3][2], lastobj;
extern float objectframe[256][2];
extern float itemyloc, fpsr, fps, clouddeg;
extern int cloudson, dontdrawover, itemselon, curitem, itemticks;
extern int selenemyon, curenemy, forcepause, attacking;
extern int eventtextstart, eventtextend;
extern float postinfo[21][3];
extern int nposts;
extern float floattext[MAX_FLOATING_TEXTS][4], floaticon[MAX_FLOATING_TEXTS][4];
extern char *floatstri[MAX_FLOATING_TEXTS];
extern char eventtext[128];
extern int invmap[4][7][13];
extern int ticks;

#ifdef OPENDINGUX
#define SDL_BLITVIDEO(X, Y, C, F) SDL_BlitSurface((X), (Y), (C), (F))
#else
#define SDL_BLITVIDEO(X, Y, C, F) sdl_blitscale((X), (Y), (C), NULL)
#endif

static float render_rnd(void)
{
	return (float)rand() * (1.0 / ((float)RAND_MAX + 1.0));
}
#define RND() render_rnd()

void sdl_blitscale(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
	static SDL_Surface *scaled;
	(void)srcrect;
	(void)dstrect;

	if(src->w == dst->w && src->h == dst->h) {
		SDL_BlitSurface(src, NULL, dst, NULL);
		return;
	}

	if(!scaled || scaled->w != dst->w || scaled->h != dst->h ||
		scaled->format->BitsPerPixel != src->format->BitsPerPixel) {
		if(scaled) SDL_FreeSurface(scaled);
		scaled = SDL_CreateRGBSurface(SDL_SWSURFACE, dst->w, dst->h,
			src->format->BitsPerPixel, src->format->Rmask, src->format->Gmask,
			src->format->Bmask, src->format->Amask);
	}
	if(!scaled) return;

	SDL_LockSurface(src);
	SDL_LockSurface(scaled);
	int bpp = src->format->BytesPerPixel;

	for(int y = 0; y < dst->h; y++) {
		Uint8 *source = (Uint8 *)src->pixels + (y * src->h / dst->h) * src->pitch;
		Uint8 *target = (Uint8 *)scaled->pixels + y * scaled->pitch;

		for(int x = 0; x < dst->w; x++)
			memcpy(target + x * bpp, source + (x * src->w / dst->w) * bpp, bpp);
	}

	SDL_UnlockSurface(scaled);
	SDL_UnlockSurface(src);
	SDL_SetAlpha(scaled, src->flags & SDL_SRCALPHA, src->format->alpha);
	SDL_BlitSurface(scaled, NULL, dst, NULL);
}

void game_drawanims(int Layer)
{
	for(int sx = 0; sx <= 19; sx++) {
		for(int sy = 0; sy <= 14; sy++) {
			int o = objmap[sx][sy];

			if(o > -1) {
				int xtiles = objectinfo[o][1];
				int ytiles = objectinfo[o][2];
				int cframe = objectframe[o][1];

				for(int x = 0; x <= xtiles - 1; x++) {
					for(int y = 0; y <= ytiles - 1; y++) {
						int x1 = (sx + x) * 16;
						int y1 = (sy + y) * 16;

						if(objecttile[o][cframe][x][y][1] == Layer) {
							int c = objecttile[o][cframe][x][y][0];
							c = c - 1;
							int curtilel = 3;
							int curtilex = c % 20;
							int curtiley = (c - curtilex) / 20;

							if(curmap == 58 && scriptflag[60][0] > 0) curtilex = 1;
							if(curmap == 54 && scriptflag[60][0] > 1) curtilex = 1;
							rcSrc.x = curtilex * 16;
							rcSrc.y = curtiley * 16;
							rcSrc.w = 16;
							rcSrc.h = 16;

							rcDest.x = x1;
							rcDest.y = y1;
							rcDest.w = 16;
							rcDest.h = 16;

							SDL_BlitSurface(tiles[curtilel], &rcSrc, videobuffer, &rcDest);
						}

						if(Layer == 1) {
							for(int l = 1; l <= 2; l++) {
								int c = tileinfo[l][sx + x][sy + y][0];
								if(c > 0) {
									int cl = tileinfo[l][sx + x][sy + y][1];

									c = c - 1;
									int curtile = c;
									int curtilel = cl;
									int curtilex = c % 20;
									int curtiley = (c - curtilex) / 20;

									rcSrc.x = curtilex * 16;
									rcSrc.y = curtiley * 16;
									rcSrc.w = 16;
									rcSrc.h = 16;

									rcDest.x = (sx + x) * 16;
									rcDest.y = (sy + y) * 16;
									rcDest.w = 16;
									rcDest.h = 16;

									int pass = 1;
									if(curtilel == 1) {
										for(int ff = 0; ff <= 5; ff++) {
											int ffa = 20 * 5 - 1 + ff * 20;
											int ffb = 20 * 5 + 4 + ff * 20;
											if(curtile > ffa && curtile < ffb) pass = 0;
										}
									}

									if(pass == 1) SDL_BlitSurface(tiles[curtilel], &rcSrc, videobuffer, &rcDest);
								}
							}
						}
					}
				}
			}
		}
	}
}

int hud_recalc(int a, int b, int c)
{
	int result = a * b / c;

	return result > b ? b : result;
}

#define RGB(R, G, B) (SDL_MapRGB(videobuffer->format, (R), (G), (B)))

void game_drawhud()
{
	char line[128];

	if(config.showdbginfo == 1) debug_drawhud();

	long ccc;

	game_fillrect(videobuffer2, 0, 0, 320, 240, 0);

	for(int i = 0; i < MAX_FLOATING_TEXTS; i++) {
		if(floattext[i][0] > 0) {
			int fc = (int)floattext[i][3];
			int c = fc, c2 = 3;

			if(fc == 4) c = 1; else if(fc == 5) c = 0;

			if(fc == 1 || fc == 3) c2 = 2;

			if(fc != 0) {
				sys_print(videobuffer, floatstri[i], (int)(floattext[i][1]) + 0, (int)(floattext[i][2]) - 1, c2);
				sys_print(videobuffer, floatstri[i], (int)(floattext[i][1]) + 0, (int)(floattext[i][2]) + 1, c2);
				sys_print(videobuffer, floatstri[i], (int)(floattext[i][1]) - 1, (int)(floattext[i][2]) + 0, c2);
				sys_print(videobuffer, floatstri[i], (int)(floattext[i][1]) + 1, (int)(floattext[i][2]) + 0, c2);
			}

			sys_print(videobuffer, floatstri[i], (int)(floattext[i][1]), (int)(floattext[i][2]), c);
		}

		if(floaticon[i][0] > 0) {
			int ico = floaticon[i][3];
			int ix = floaticon[i][1];
			int iy = floaticon[i][2];

			rcDest.x = ix;
			rcDest.y = iy;

			if(ico != 99) SDL_BlitSurface(itemimg[ico], NULL, videobuffer, &rcDest);
			if(ico == 99) {
				SDL_SetAlpha(spellimg, SDL_SRCALPHA, (int)(RND() * 96) + 96);

				rcSrc.x = 16 * (int)(RND() * 2);
				rcSrc.y = 80;
				rcSrc.w = 16;
				rcSrc.h = 16;

				rcDest.x = ix;
				rcDest.y = iy;

				SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);

				SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);
			}
		}
	}

	if(itemselon == 0) {
		int sy = 211;


		int nx = 19 * 8 + 13;
		rcSrc.x = nx - 17 + 48;
		rcSrc.y = sy;

		// spells in game
		if(player.foundspell[0] == 1) {
			for(int i = 0; i < 5; i++) {
				rcSrc.x = rcSrc.x + 17;

				if(player.foundspell[i] == 1) {
					SDL_BlitSurface(itemimg[7 + i], NULL, videobuffer, &rcSrc);

					game_fillrect(videobuffer, rcSrc.x, sy + 16, 16, 4, RGB(0, 32, 32));
					game_fillrect(videobuffer, rcSrc.x + 1, sy + 17,
							hud_recalc(player.spellcharge[i], 14, 100), 2,
							player.spellcharge[i] == 100 ? RGB(255, 128, 32) : RGB(0, 224, 64));
				}
			}
		}
		return;
	}

	if(selenemyon == 0) {
		rcDest.x = 0;
		rcDest.y = 0;
		rcDest.w = 320;
		rcDest.h = 240;
		SDL_SetAlpha(videobuffer2, SDL_SRCALPHA, (int)(player.itemselshade * 4));
		SDL_FillRect(videobuffer2, &rcDest, 0);
		SDL_BlitSurface(videobuffer2, NULL, videobuffer, NULL);

		int sy = 202;
		rcSrc.x = 46;
		rcSrc.y = 46;

		SDL_SetAlpha(inventoryimg, SDL_SRCALPHA, 160); // 128
		SDL_BlitSurface(inventoryimg, NULL, videobuffer, &rcSrc);
		SDL_SetAlpha(inventoryimg, SDL_SRCALPHA, 255);

		int sx = 54;
		sy = 55;

		// draw map 9,77
		rcDest.x = 46 + 9;
		rcDest.y = 46 + 77;

		int amap = 0;
		if(curmap > 46) amap = 2;
		if(curmap > 67) amap = 3;
		if(curmap > 5 && curmap < 42) amap = 1;
		SDL_BlitSurface(mapimg[amap], NULL, videobuffer, &rcDest);

		ccc = SDL_MapRGB(videobuffer->format, 128 + 127 * sin(3.141592 * 2 * itemyloc / 16), 0, 0);

		for(int b = 0; b <= 6; b++) {
			for(int a = 0; a <= 12; a++) {
				if(invmap[amap][b][a] == curmap) {
					game_fillrect(videobuffer, 46 + 9 + a * 9 + 2, 46 + 77 + b * 9 + 1, 6, 6, ccc);
				}
			}
		}

		if(amap == 1) {
			sys_print(videobuffer, "L1", 46 + 9, 46 + 77, 0);
			sys_print(videobuffer, "L2", 46 + 9 + 7 * 9, 46 + 77, 0);
		}

		sprintf(line, "Health: %i/%i", player.hp, player.maxhp);
		sys_print(videobuffer, line, sx, sy, player.hp <= player.maxhp * 0.25 ? (int)player.hpflash : 0);

		sprintf(line, "Level : %i", player.level);
		if(player.level == player.maxlevel) strcpy(line, "Level : MAX");
		sys_print(videobuffer, line, sx, sy + 8, 0);

		// experience
		game_fillrect(videobuffer, sx + 64, sy + 16, 16, 4, RGB(0, 32, 32));
		game_fillrect(videobuffer, sx + 65, sy + 17,
				hud_recalc(player.exp, 14, player.nextlevel), 2, RGB(0, 224, 64));

		// attack strength
		game_fillrect(videobuffer, sx + 1, sy + 16, 56, 6, RGB(0, 32, 32));
		game_fillrect(videobuffer, sx + 1, sy + 17,
				hud_recalc(player.attackstrength, 54, 100), 2,
				player.attackstrength == 100 ? RGB(255, 128, 32) : RGB(0, 64, 224));

		// spell strength
		game_fillrect(videobuffer, sx + 1, sy + 19,
				hud_recalc(player.spellstrength, 54, 100), 2,
				player.spellstrength == 100 ? RGB(224, 0, 0) : RGB(128, 0, 224));

		sprintf(line, "S:%i M:%i D:%i%%", player.sworddamage, player.spelldamage,
			(player.shield + player.armour) * 5);
		sys_print(videobuffer, line, 54, 80, 0);

		// time
		int ase = secstart + secsingame;
		int h = ((ase - (ase % 3600)) / 3600);
		ase = (ase - h * 3600);
		int m = ((ase - (ase % 60)) / 60);
		int s = (ase - m * 60);

		sprintf(line, "%02i:%02i:%02i", h, m, s);
		sys_print(videobuffer, line, 46 + 38 - strlen(line) * 4, 46 + 49, 0);

		sys_print(videobuffer, "Use", 193, 55, 0);
		sys_print(videobuffer, "Cast", 236, 55, 0);

		rcSrc.x = 128;
		rcSrc.y = 91;

		int equipment[3] = { player.sword, player.shield, player.armour };
		for(int i = 0; i < 3; i++) {
			int ss = equipment[i] == 3 ? 18 + i : (equipment[i] - 1) * 3 + i;
			rcSrc.x = 128 + i * 16;
			SDL_BlitSurface(itemimg[ss], NULL, videobuffer, &rcSrc);
		}

		for(int i = 0; i <= 4; i++) {
			sx = 188;
			sy = 70 + i * 24;
			rcSrc.x = sx;
			rcSrc.y = sy;
			if(i == 0) SDL_BlitSurface(itemimg[6], NULL, videobuffer, &rcSrc);
			if(i == 1) SDL_BlitSurface(itemimg[12], NULL, videobuffer, &rcSrc);
			if(i == 2) SDL_BlitSurface(itemimg[17], NULL, videobuffer, &rcSrc);
			if(i == 3) SDL_BlitSurface(itemimg[16], NULL, videobuffer, &rcSrc);
			if(i == 4) SDL_BlitSurface(itemimg[14], NULL, videobuffer, &rcSrc);

			sprintf(line, "x%i", player.inventory[i]);
			sys_print(videobuffer, line, sx + 17, sy + 7, 0);
		}

		// spells in menu
		if(player.foundspell[0] == 1) {
			for(int i = 0; i < 5; i++) {
				rcSrc.x = 243;
				rcSrc.y = 67 + i * 24;
				sy = rcSrc.y;

				if(player.foundspell[i] == 1) {
					SDL_BlitSurface(itemimg[7 + i], NULL, videobuffer, &rcSrc);

					game_fillrect(videobuffer, rcSrc.x, sy + 16, 16, 4, RGB(0, 32, 32));
					game_fillrect(videobuffer, rcSrc.x + 1, sy + 17,
							hud_recalc(player.spellcharge[i], 14, 100), 2,
							player.spellcharge[i] == 100 ? RGB(255, 128, 32) : RGB(0, 224, 64));
				}
			}
		}

		if(itemselon == 1) {
			for(int i = 0; i <= 4; i++) {
				if(curitem == 5 + i) {
					rcDest.x = (float)(243 - 12 + 3 * sin(3.141592 * 2 * itemyloc / 16));
					rcDest.y = 67 + 24 * i;
					SDL_BlitSurface(itemimg[15], NULL, videobuffer, &rcDest);
				}

				if(curitem == i) {
					rcDest.x = (float)(189 - 12 + 3 * sin(3.141592 * 2 * itemyloc / 16));
					rcDest.y = 70 + 24 * i;
					SDL_BlitSurface(itemimg[15], NULL, videobuffer, &rcDest);
				}
			}
		}
	}

	if(selenemyon == 1) {
		if(curenemy > lastnpc) {
			int pst = curenemy - lastnpc - 1;
			rcDest.x = postinfo[pst][0];
			rcDest.y = (float)(postinfo[pst][1] - 4 - sin(3.141592 / 8 * itemyloc));
		} else {
			rcDest.x = npcinfo[curenemy].x + 4;
			rcDest.y = (float)(npcinfo[curenemy].y + 4 - 16 - sin(3.141592 / 8 * itemyloc));
		}

		SDL_BlitSurface(itemimg[13], NULL, videobuffer, &rcDest);
	}
}

void game_drawover(int modx, int mody)
{
	int npx = modx + 12;
	int npy = mody + 20;

	int lx = (int)npx / 16;
	int ly = (int)npy / 16;

	for(int xo = -1; xo <= 1; xo++) {
		for(int yo = -1; yo <= 1; yo++) {

			int sx = lx + xo;
			int sy = ly + yo;

			int sx2 = sx * 16;
			int sy2 = sy * 16;

			if(sx > -1 && sx < 40 && sy > -1 && sy < 24) {

				int curtile = tileinfo[2][sx][sy][0];
				int curtilel = tileinfo[2][sx][sy][1];

				if(curtile > 0) {
					curtile = curtile - 1;
					int curtilex = curtile % 20;
					int curtiley = (curtile - curtilex) / 20;

					rcSrc.x = curtilex * 16;
					rcSrc.y = curtiley * 16;
					rcSrc.w = 16;
					rcSrc.h = 16;

					rcDest.x = sx2;
					rcDest.y = sy2;
					rcDest.w = 16;
					rcDest.h = 16;

					int pass = 1;
					if(curtilel == 1) {
						for(int ff = 0; ff <= 5; ff++) {
							int ffa = 20 * 5 - 1 + ff * 20;
							int ffb = 20 * 5 + 4 + ff * 20;
							if(curtile > ffa && curtile < ffb) pass = 0;
						}
					}

					if(pass == 1) SDL_BlitSurface(tiles[curtilel], &rcSrc, videobuffer, &rcDest);
				}
			}
		}
	}
}

void game_drawplayer()
{
	long ccc;

	int f = 0;
	if(player.armour == 3) f = 13;

	if(attacking == 0) {
		rcSrc.x = (int)(player.walkframe / 4) * 24;
		rcSrc.y = player.walkdir * 24;
		rcSrc.w = 24;
		rcSrc.h = 24;

		rcDest.x = (int)(player.px);
		rcDest.y = (int)(player.py);
		rcDest.w = 24;
		rcDest.h = 24;

		SDL_BlitSurface(anims[f], &rcSrc, videobuffer, &rcDest);
	} else {
		rcSrc.x = (int)(player.attackframe / 4) * 24;
		rcSrc.y = player.walkdir * 24;
		rcSrc.w = 24;
		rcSrc.h = 24;

		rcDest.x = (int)(player.px);
		rcDest.y = (int)(player.py);
		rcDest.w = 24;
		rcDest.h = 24;

		SDL_BlitSurface(animsa[f], &rcSrc, videobuffer, &rcDest);

	}

	ccc = SDL_MapRGB(videobuffer->format, 224, 224, 64);

	int pass = 0;
	if(player.hp <= player.maxhp * 0.25) pass = 1;

	if(pass == 1) {
		ccc = SDL_MapRGB(videobuffer->format, 255, 255, 255);
		if((int)(player.hpflash) == 1) ccc = SDL_MapRGB(videobuffer->format, 255, 0, 0);
	}

	int sss = 6;
	if(player.foundspell[0]) sss = 8;
	int npx = player.px;
	int npy = player.py;
	rcDest.x = npx + 4;
	rcDest.y = npy + 22;
	rcDest.w = 16;
	rcDest.h = sss;

	SDL_FillRect(videobuffer, &rcDest, 0);

	rcDest.x = npx + 5;
	rcDest.y = npy + 23;


	int ww = 14 * player.hp / player.maxhp;
	if(ww > 14) ww = 14;
	if(ww < 1) ww = 1;

	rcDest.w = ww;
	rcDest.h = 2;

	SDL_FillRect(videobuffer, &rcDest, ccc);

	ccc = SDL_MapRGB(videobuffer->format, 0, 224, 64);
	if(player.attackstrength == 100) ccc = SDL_MapRGB(videobuffer->format, 255, 128, 32);

	ww = 14 * player.attackstrength / 100;
	if(ww > 14) ww = 14;

	int ww2 = 14 * player.spellstrength / 100;
	if(ww2 > 14) ww2 = 14;

	rcDest.w = ww;
	rcDest.h = 2;
	rcDest.y = rcDest.y + 2;

	SDL_FillRect(videobuffer, &rcDest, ccc);

	ccc = SDL_MapRGB(videobuffer->format, 128, 0, 224);
	if(player.spellstrength == 100) ccc = SDL_MapRGB(videobuffer->format, 224, 0, 0);

	rcDest.w = ww2;
	rcDest.h = 2;
	rcDest.y = rcDest.y + 2;

	SDL_FillRect(videobuffer, &rcDest, ccc);
}

void game_drawview()
{
	SDL_Rect rc;

	SDL_BlitSurface(mapbg, NULL, videobuffer, NULL);

	game_updspellsunder();

	game_drawanims(0);

	// ------dontdrawover = special case to make boss work right in room 24
	if(dontdrawover == 1) game_drawanims(1);
	game_drawnpcs(0);

	game_drawplayer();

	game_drawnpcs(1);
	if(dontdrawover == 0) game_drawanims(1);

	game_drawover((int)player.px, (int)player.py);

	game_updspells();

	if(cloudson == 1) {
		rc.x = (float)(256 + 256 * cos(3.141592 / 180 * clouddeg));
		rc.y = (float)(192 + 192 * sin(3.141592 / 180 * clouddeg));
		rc.w = 320;
		rc.h = 240;

		SDL_BlitSurface(cloudimg, &rc, videobuffer, NULL);
	}

	game_drawhud();
	game_draweventtext();

	SDL_BLITVIDEO(videobuffer, NULL, video, NULL);
}

void game_draweventtext()
{
	if(eventtextend <= ticks || eventtext[0] == '\0') return;

	int elapsed = ticks - eventtextstart;
	int remaining = eventtextend - ticks;
	int alpha = 192;

	if(elapsed < 500) alpha = 192 * elapsed / 500;
	if(remaining < 500) alpha = 192 * remaining / 500;
	if(alpha < 0) alpha = 0;
	if(alpha > 192) alpha = 192;

	SDL_SetAlpha(windowimg, SDL_SRCALPHA, alpha);
	SDL_BlitSurface(windowimg, NULL, videobuffer, NULL);
	SDL_SetAlpha(windowimg, SDL_SRCALPHA, 255);

	if(elapsed >= 500) {
		int x = 160 - 4 * strlen(eventtext);
		sys_print(videobuffer, eventtext, x, 15, 0);
	}
}


