#include <math.h>
#include <stdlib.h>
#include <SDL/SDL_mixer.h>

#include "griffon.h"
#include "config.h"
#include "debug.h"
#include "npc.h"

#undef abs
#define abs(x) ((x) < 0 ? -(x) : (x))

extern SDL_Surface *videobuffer, *clipbg, *spellimg;
extern SDL_Surface *anims[100], *animsa[100];
extern SDL_Rect rcSrc, rcDest;
extern NPCTYPE npcinfo[MAXNPC];
extern ANIMSET2TYPE animset2[7], animset9[7];
extern int lastnpc, firsty, lasty, ysort[2401];
extern int ticks, game_time;
extern float fpsr, itemyloc;
extern int triggerloc[320][240];
extern int menabled;
extern Mix_Chunk *sfx[21];

static float npc_rnd(void)
{
	return (float)rand() * (1.0 / ((float)RAND_MAX + 1.0));
}
#define RND() npc_rnd()

static void game_drawbar(int x, int y, int value, int max, unsigned int color)
{
	game_fillrect(videobuffer, x, y, 16, 4, 0);
	if(value <= 0 || max <= 0) return;
	int width = 14;
	if(max > 0 && value < max) width = 14 * value / max;
	if(width < 1) width = 1;
	if(width > 14) width = 14;
	game_fillrect(videobuffer, x + 1, y + 1, width, 2, color);
}

static void npcshake(NPCTYPE *npc, SDL_Rect *dest)
{
	if(npc->pause > game_time && npc->shake < game_time) {
		npc->shake = game_time + 50;
		dest->x += (int)(RND() * 3) - 1;
		dest->y += (int)(RND() * 3) - 1;
	}
}

static void npcsprite24(NPCTYPE *npc, SDL_Surface *image, int x, int y, int frame, int shake)
{
	rcSrc.x = (frame / 4) * 24;
	rcSrc.y = npc->walkdir * 24;
	rcSrc.w = 24;
	rcSrc.h = 24;

	rcDest.x = x;
	rcDest.y = y;
	rcDest.w = 24;
	rcDest.h = 24;

	if(shake) npcshake(npc, &rcDest);
	SDL_BlitSurface(image, &rcSrc, videobuffer, &rcDest);
}

static void npcserpentdraw(NPCTYPE *npc, SDL_Surface *image, ANIMSET2TYPE *animset)
{
	for(int f = 0; f <= 7; f++) {
		int s = npc->bodysection[f].sprite;
		int yp = 0;
		if(npc->spriteset == 9 && f == 0 &&
			(curmap == 53 || curmap == 57 || curmap == 61 || curmap == 65 || curmap == 56 || curmap > 66) &&
			scriptflag[60][0] > 0) yp = 16;

		rcSrc.x = animset[s].x;
		rcSrc.y = animset[s].y + yp;
		rcSrc.w = animset[s].w;
		rcSrc.h = animset[s].h;
		rcDest.x = npc->bodysection[f].x - animset[s].xofs;
		rcDest.y = npc->bodysection[f].y - animset[s].yofs + 2;
		SDL_BlitSurface(image, &rcSrc, videobuffer, &rcDest);
	}
}

static void npcserpentsections(NPCTYPE *npc, int divisor)
{
	npc->bodysection[0].x = npc->x + 12;
	npc->bodysection[0].y = npc->y + 12;
	for(int f = 6; f >= 1; f--) {
		float xdif = npc->bodysection[f + 1].x - npc->bodysection[f - 1].x;
		float ydif = npc->bodysection[f + 1].y - npc->bodysection[f - 1].y;
		float tx = npc->bodysection[f - 1].x + xdif / 2;
		float ty = npc->bodysection[f - 1].y + ydif / 2;
		npc->bodysection[f].x -= (npc->bodysection[f].x - tx) / divisor;
		npc->bodysection[f].y -= (npc->bodysection[f].y - ty) / divisor;
	}
}

static void npcserpentattack(NPCTYPE *npc)
{
	float xdif = npc->bodysection[7].x - npc->headtargetx[0];
	float ydif = npc->bodysection[7].y - npc->headtargety[0];
	npc->bodysection[7].x -= xdif * 0.4 * fpsr;
	npc->bodysection[7].y -= ydif * 0.4 * fpsr;
	npcserpentsections(npc, 1);

	npc->attackframe += npc->attackspd * fpsr;
	if(npc->attackframe >= 16) {
		npc->attackframe = 0;
		npc->attacking = 0;
		npc->attacknext = game_time + npc->attackdelay;
	}
	npc->cattackframe = (int)npc->attackframe;

	float npx = npc->bodysection[7].x;
	float npy = npc->bodysection[7].y + 16;
	xdif = (player.px + 12) - npx;
	ydif = (player.py + 12) - npy;
	if(abs(xdif) < 8 && abs(ydif) < 8 && player.pause < game_time) {
		npc->attacknext = game_time + npc->attackdelay;
		int damage = npc->attackdamage * (1.0 + RND() * 0.5);
		if(player.hp > 0) game_damageplayer(damage);
	}
}

void game_drawnpcs(int mode)
{
	unsigned int ccc;

	ccc = SDL_MapRGB(videobuffer->format, 255, 128, 32);

	int fst = firsty;
	int lst = lasty;

	if(mode == 0) lst = player.ysort;
	if(mode == 1) fst = player.ysort;

	for(int yy = fst; yy <= lst; yy++) {

		if(ysort[yy] > 0) {
			int i = ysort[yy];

			if(npcinfo[i].hp > 0) {
				int npx = (int)(npcinfo[i].x);
				int npy = (int)(npcinfo[i].y);

				int sprite = npcinfo[i].spriteset;

				int wdir = npcinfo[i].walkdir;

				// spriteset1 specific
				if(npcinfo[i].spriteset == 1) {
					if(npcinfo[i].attacking == 0)
						npcsprite24(&npcinfo[i], anims[sprite], npx, npy, npcinfo[i].cframe, 1);
					else
						npcsprite24(&npcinfo[i], animsa[sprite], npx, npy, npcinfo[i].cattackframe, 0);
				}

				if(npcinfo[i].spriteset == 2) npcserpentdraw(&npcinfo[i], anims[2], animset2);
				if(npcinfo[i].spriteset == 9) npcserpentdraw(&npcinfo[i], anims[9], animset9);


				// boss 1 and black knight
				if(npcinfo[i].spriteset == 3 || npcinfo[i].spriteset == 4) {
					int frame = npcinfo[i].attacking ? 16 : npcinfo[i].cframe;
					rcSrc.x = (frame / 4) * 24;
					rcSrc.y = 0;
					rcSrc.w = 24;
					rcSrc.h = 48;

					rcDest.x = npx - 2;
					rcDest.y = npy - 24;

					SDL_BlitSurface(anims[sprite], &rcSrc, videobuffer, &rcDest);
				}


				// firehydra
				if(npcinfo[i].spriteset == 5) {
					for(int ff = 0; ff <= 2; ff++) {
						if(npcinfo[i].hp > 10 * ff * 20) {
							rcSrc.x = 16 * (int)(RND() * 2);
							rcSrc.y = 80;
							rcSrc.w = 16;
							rcSrc.h = 16;

							rcDest.x = npcinfo[i].bodysection[10 * ff].x - 8;
							rcDest.y = npcinfo[i].bodysection[10 * ff].y - 8;

							int x = 192 + ((int)(itemyloc + ff * 5) % 3) * 64;
							if(x > 255) x = 255;
							SDL_SetAlpha(spellimg, SDL_SRCALPHA, x);
							SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);
							SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);

							for(int f = 1; f <= 8; f++) {
								rcSrc.x = 16 * (int)(RND() * 2);
								rcSrc.y = 80;
								rcSrc.w = 16;
								rcSrc.h = 16;

								rcDest.x = npcinfo[i].bodysection[ff * 10 + f].x - 8 + (int)(RND() * 3) - 1;
								rcDest.y = npcinfo[i].bodysection[ff * 10 + f].y - 8 + (int)(RND() * 3) - 1;

								x = 192 + f % 3 * 64;
								if(x > 255) x = 255;
								SDL_SetAlpha(spellimg, SDL_SRCALPHA, x);
								SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);
								SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);
							}

							rcSrc.x = 0;
							rcSrc.y = 0;
							rcSrc.w = 42;
							rcSrc.h = 36;

							rcDest.x = npcinfo[i].bodysection[10 * ff + 9].x - 21;
							rcDest.y = npcinfo[i].bodysection[10 * ff + 9].y - 21;

							SDL_SetAlpha(spellimg, SDL_SRCALPHA, 192);
							SDL_BlitSurface(anims[5], &rcSrc, videobuffer, &rcDest);
							SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);

						}

					}

				}

				// red dragon
				if(npcinfo[i].spriteset == 6) {
					npcsprite24(&npcinfo[i], anims[sprite], npx, npy, npcinfo[i].cframe, 1);
				}

				// wizard
				if(npcinfo[i].spriteset == 7)
					npcsprite24(&npcinfo[i], anims[sprite], npx, npy, npcinfo[i].cframe, 1);


				// yellow dragon
				if(npcinfo[i].spriteset == 8) {
					npcsprite24(&npcinfo[i], anims[sprite], npx, npy, npcinfo[i].cframe, 1);
				}


				// dragon2
				if(npcinfo[i].spriteset == 10) {
					if(npcinfo[i].attacking == 0) {
						npcinfo[i].floating = npcinfo[i].floating + 0.25 * fpsr;
						while(npcinfo[i].floating >= 16) npcinfo[i].floating = npcinfo[i].floating - 16;

						float frame = npcinfo[i].frame;
						int cframe = npcinfo[i].cframe;

						frame = frame + 0.5 * fpsr;
						while(frame >= 16)
							frame = frame - 16;

						cframe = (int)(frame);
						if(cframe > 16) cframe = 16 - 1;
						if(cframe < 0) cframe = 0;

						npcinfo[i].frame = frame;
						npcinfo[i].cframe = cframe;

						cframe = npcinfo[i].cframe;

						rcSrc.x = 74 * wdir;
						rcSrc.y = (int)(cframe / 4) * 48;
						rcSrc.w = 74;
						rcSrc.h = 48;

						rcDest.x = npx + 12 - 37;
						rcDest.y = (float)(npy + 12 - 32 - 3 * sin(3.141592 * 2 * npcinfo[i].floating / 16));
						rcDest.w = 24;
						rcDest.h = 24;

						npcshake(&npcinfo[i], &rcDest);

						SDL_BlitSurface(anims[sprite], &rcSrc, videobuffer, &rcDest);
					} else {
						npcinfo[i].floating = npcinfo[i].floating + 0.25 * fpsr;
						while(npcinfo[i].floating >= 16) npcinfo[i].floating = npcinfo[i].floating - 16;

						int cframe = npcinfo[i].cattackframe;

						rcSrc.x = 74 * wdir;
						rcSrc.y = (int)(cframe / 4) * 48;
						rcSrc.w = 74;
						rcSrc.h = 48;

						rcDest.x = npx + 12 - 37;
						rcDest.y = (float)(npy + 12 - 32 - 3 * sin(3.141592 * 2 * npcinfo[i].floating / 16));
						rcDest.w = 24;
						rcDest.h = 24;

						SDL_BlitSurface(animsa[sprite], &rcSrc, videobuffer, &rcDest);
					}
				}

				// end boss
				if(npcinfo[i].spriteset == 11) {

					npcinfo[i].floating = npcinfo[i].floating + .3 * fpsr;
					while(npcinfo[i].floating >= 16) npcinfo[i].floating = npcinfo[i].floating - 16;


					float frame = npcinfo[i].frame2;

					frame = frame + 0.5 * fpsr;
					while(frame >= 16)
						frame = frame - 16;

					npcinfo[i].frame2 = frame;

					int sx = npx + 12 - 40;
					int sy = (float)(npy + 12 - 50 - 3 * sin(3.141592 * 2 * npcinfo[i].floating / 16));

					for(int fr = 0; fr <= 3; fr++) {
						SDL_SetAlpha(spellimg, SDL_SRCALPHA, 128 + (int)(RND() * 96));

						rcSrc.x = 16 * (int)(RND() * 2);
						rcSrc.y = 80;
						rcSrc.w = 16;
						rcSrc.h = 16;

						rcDest.x = sx + 32 + (int)(RND() * 3) - 1;
						rcDest.y = sy - (int)(RND() * 6);

						SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);
					}

					for(int ii = 0; ii <= 8; ii++) {
						for(int i2 = 0; i2 <= 3; i2++) {
							rcSrc.x = 16 * (int)(RND() * 2);
							rcSrc.y = 80;
							rcSrc.w = 16;
							rcSrc.h = 16;

							float fr3 = frame - 3 + i2;
							if(fr3 < 0) fr3 = fr3 + 16;

							rcDest.x = (float)(sx + 36 + ii * 8 - ii * cos(3.14159 * 2 * (fr3 - ii) / 16) * 2);
							rcDest.y = (float)(sy + 16 + ii * sin(3.14159 * 2 * (fr3 - ii) / 16) * 3 - ii); //  * 4

							SDL_SetAlpha(spellimg, SDL_SRCALPHA, i2 / 3 * 224);

							SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);

							int xloc = rcDest.x;
							int yloc = rcDest.y;
							int xdif = (xloc + 8) - (player.px + 12);
							int ydif = (yloc + 8) - (player.py + 12);

							if((abs(xdif) < 8 && abs(ydif) < 8) && player.pause < game_time) {
								float damage = (float)npcinfo[i].spelldamage * (1.0 + RND() * 0.5);

								if(player.hp > 0) {
									game_damageplayer(damage);
									game_playsound(sndfire);
								}

							}


							rcDest.x = (float)(sx + 36 - ii * 8 + ii * cos(3.14159 * 2 * (fr3 - ii) / 16) * 2);
							rcDest.y = (float)(sy + 16 + ii * sin(3.14159 * 2 * (fr3 - ii) / 16) * 3 - ii); //  * 4

							SDL_SetAlpha(spellimg, SDL_SRCALPHA, i2 / 3 * 224);

							SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);

							xloc = rcDest.x;
							yloc = rcDest.y;
							xdif = (xloc + 8) - (player.px + 12);
							ydif = (yloc + 8) - (player.py + 12);

							if((abs(xdif) < 8 && abs(ydif) < 8) && player.pause < game_time) {
								float damage = (float)npcinfo[i].spelldamage * (1.0 + RND() * 0.5);

								if(player.hp > 0) {
									game_damageplayer(damage);
									game_playsound(sndfire);
								}
							}
						}
					}

					SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);

					if(npcinfo[i].attacking == 0) {
						int cframe = (int)(frame);
						rcSrc.x = 0;
						rcSrc.y = 72 * (int)(cframe / 4);
						rcSrc.w = 80;
						rcSrc.h = 72;

						rcDest.x = sx;
						rcDest.y = sy;

						npcshake(&npcinfo[i], &rcDest);

						SDL_BlitSurface(anims[sprite], &rcSrc, videobuffer, &rcDest);
					} else {
						int cframe = (int)(npcinfo[i].cattackframe);

						rcSrc.x = 0;
						rcSrc.y = 72 * (int)(cframe / 4);
						rcSrc.w = 80;
						rcSrc.h = 72;

						rcDest.x = sx;
						rcDest.y = sy;

						SDL_BlitSurface(animsa[sprite], &rcSrc, videobuffer, &rcDest);
					}
				}

				// bat kitty
				if(npcinfo[i].spriteset == 12) {
					npcinfo[i].floating = npcinfo[i].floating + 1 * fpsr;
					while(npcinfo[i].floating >= 16) npcinfo[i].floating = npcinfo[i].floating - 16;

					float frame = npcinfo[i].frame;
					int cframe = npcinfo[i].cframe;

					frame = frame + 0.5 * fpsr;
					while(frame >= 16)
						frame = frame - 16;

					cframe = (int)(frame);
					if(cframe > 16) cframe = 16 - 1;
					if(cframe < 0) cframe = 0;

					npcinfo[i].frame = frame;
					npcinfo[i].cframe = cframe;

					cframe = npcinfo[i].cframe;

					rcSrc.x = 0;
					rcSrc.y = 0;
					rcSrc.w = 99;
					rcSrc.h = 80;

					rcDest.x = npx + 12 - 50;
					rcDest.y = (float)(npy + 12 - 64 + 2 * sin(3.141592 * 2 * npcinfo[i].floating / 16));
					rcDest.w = 99;
					rcDest.h = 80;

					npcshake(&npcinfo[i], &rcDest);

					SDL_BlitSurface(anims[sprite], &rcSrc, videobuffer, &rcDest);
				}

				game_drawbar(npx + 4, npy + 22, npcinfo[i].hp, npcinfo[i].maxhp, ccc);

				if(i == controlled_npc) {
					for(int bar = 0; bar < 2; bar++) {
						int remaining = (bar == 0 ? npcinfo[i].specialnext : npcinfo[i].attacknext) - game_time;
						int cooldown = npcinfo[i].attackdelay * (bar == 0 ? 2 : 1);
						int charge = cooldown;
						if(remaining > 0 && cooldown > 0) charge = cooldown - remaining;
						game_drawbar(npx + 4, npy + 25 + bar * 3, charge, cooldown,
							SDL_MapRGB(videobuffer->format, bar == 0 ? 64 : 224,
								bar == 0 ? 192 : 128, bar == 0 ? 255 : 32));
					}
				}

				int pass = 1;

				if(npcinfo[i].spriteset == 3) pass = 0;
				if(pass == 1) game_drawover(npx, npy);

			}
		}
	}
}

void game_updnpcs()
{
	float npx, npy, onpx, onpy;
	unsigned int *temp, dq, bgc;
	float nnxa = 0, nnya = 0, nnxb = 0, nnyb = 0;
	int pass = 0, checkpass;
	float wspd, xdif, ydif;
	int sx, sy;
	int dospell = 0;
	float realpx = player.px, realpy = player.py;
	int realpause = player.pause;

	for(int i = 1; i <= lastnpc; i++) {
		player.px = realpx;
		player.py = realpy;
		player.pause = realpause;
		ai_target = 0;

		if((npc_infight || npcinfo[i].aggro) && i != controlled_npc && npcinfo[i].hp > 0) {
			float best = 1000000;
			if(npc_infight) {
				for(int e = 1; e <= lastnpc; e++) {
					if(e == i || npcinfo[e].hp <= 0) continue;
					float dx = npcinfo[e].x - npcinfo[i].x;
					float dy = npcinfo[e].y - npcinfo[i].y;
					float distance = dx * dx + dy * dy;
					if(distance < best) {
						best = distance;
						ai_target = e;
					}
				}
			} else if(controlled_npc > 0 && npcinfo[controlled_npc].hp > 0) {
				ai_target = controlled_npc;
			}
			if(ai_target > 0) {
				player.px = npcinfo[ai_target].x;
				player.py = npcinfo[ai_target].y;
				player.pause = npcinfo[ai_target].pause;
			}
		}

		if(i == controlled_npc) continue;
		if(npcinfo[i].hp > 0) {
			//  is npc walking
			pass = 0;
			if(npcinfo[i].attacking == 0) pass = 1;
			if(npcinfo[i].spriteset == 5) pass = 1;
			if(pass == 1) {
				int moveup = 0;
				int movedown = 0;
				int moveleft = 0;
				int moveright = 0;

				npx = npcinfo[i].x;
				npy = npcinfo[i].y;

				onpx = npx;
				onpy = npy;

				wspd = npcinfo[i].walkspd / 4;

				if(npcinfo[i].spriteset == 10) wspd = wspd * 2;
				int wdir = npcinfo[i].walkdir;

				int mode = npcinfo[i].movementmode;

				xdif = player.px - npx;
				ydif = player.py - npy;

				if(abs(xdif) < 4 * 16 && abs(ydif) < 4 * 16 && mode < 3) mode = 0;
				if(npcinfo[i].hp < npcinfo[i].maxhp * 0.25) mode = 3;

				if(npcinfo[i].pause > game_time) mode = -1;
				if(npcinfo[i].spriteset == 2 && npcinfo[i].castpause > game_time) mode = -1;

				if(mode == 3) {
					mode = 1;
					if(abs(xdif) < 4 * 16 && abs(ydif) < 4 * 16) mode = 3;
				}

				checkpass = 0;

				// npc  AI CODE
				// --------------

				// *** aggressive
				if(mode == 0) {
					wspd = npcinfo[i].walkspd / 2;

					xdif = player.px - npx;
					ydif = player.py - npy;

					if(abs(xdif) > abs(ydif)) {
						if(xdif < 4) wdir = 2;
						if(xdif > -4) wdir = 3;
					} else {
						if(ydif < 4) wdir = 0;
						if(ydif > -4) wdir = 1;
					}

					if(xdif < 4) moveleft = 1;
					if(xdif > -4) moveright = 1;
					if(ydif < 4) moveup = 1;
					if(ydif > -4) movedown = 1;
				}
				// *******************

				// *** defensive
				if(mode == 1) {

					int movingdir = npcinfo[i].movingdir;

					if(npcinfo[i].ticks > ticks + 100000) npcinfo[i].ticks = ticks;

					if(npcinfo[i].ticks < ticks) {
						npcinfo[i].ticks = ticks + 2000;
						movingdir = (int)(RND() * 8);
						npcinfo[i].movingdir = movingdir;
					}

					if(movingdir == 0) {
						wdir = 2; // left
						moveup = 1;
						moveleft = 1;
					} else if(movingdir == 1) {
						wdir = 0; // up
						moveup = 1;
					} else if(movingdir == 2) {
						wdir = 3; // right
						moveup = 1;
						moveright = 1;
					} else if(movingdir == 3) {
						wdir = 3; // right
						moveright = 1;
					} else if(movingdir == 4) {
						wdir = 3; // right
						moveright = 1;
						movedown = 1;
					} else if(movingdir == 5) {
						wdir = 1; // down
						movedown = 1;
					} else if(movingdir == 6) {
						wdir = 2; // left
						movedown = 1;
						moveleft = 1;
					} else if(movingdir == 7) {
						wdir = 2; // left
						moveleft = 1;
					}

					checkpass = 1;
				}
				// *******************

				// *** run away
				if(mode == 3) {
					wspd = npcinfo[i].walkspd / 2;

					xdif = player.px - npx;
					ydif = player.py - npy;

					if(abs(xdif) > abs(ydif)) {
						if(xdif < 4) wdir = 3;
						if(xdif > -4) wdir = 2;
					} else {
						if(ydif < 4) wdir = 1;
						if(ydif > -4) wdir = 0;
					}

					if(xdif < 4) moveright = 1;
					if(xdif > -4) moveleft = 1;
					if(ydif < 4) movedown = 1;
					if(ydif > -4) moveup = 1;
				}
				// *******************

				// -------------- ?? move*** vs movin***
				int movinup = 0;
				int movindown = 0;
				int movinleft = 0;
				int movinright = 0;

				float xp = (npx / 2 + 6);
				float yp = (npy / 2 + 10);

				if(npcinfo[i].spriteset == 10) wspd = wspd * 2;

				float ii = wspd * fpsr;
				if(ii < 1) ii = 1;

				SDL_LockSurface(clipbg);

				if(moveup) {
					sx = xp;
					sy = yp - ii;
					temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
					dq = *temp;
					if(npcinfo[i].spriteset == 11) dq = 0;

					if(dq == 0) movinup = 1;
					if(dq > 0) {
						sx = xp - ii;
						sy = yp - ii;
						temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
						dq = *temp;
						if(npcinfo[i].spriteset == 11) dq = 0;
						if(dq == 0) {
							movinup = 1;
							movinleft = 1;
						}
					}
					if(dq > 0) {
						sx = xp + ii;
						sy = yp - ii;
						temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
						dq = *temp;
						if(npcinfo[i].spriteset == 11) dq = 0;
						if(dq == 0) {
							movinup = 1;
							movinright = 1;
						}
					}
				}

				if(movedown) {
					sx = xp;
					sy = yp + ii;
					temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
					dq = *temp;
					if(npcinfo[i].spriteset == 11) dq = 0;
					if(dq == 0) movindown = 1;
					if(dq > 0) {
						sx = xp - ii;
						sy = yp + ii;
						temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
						dq = *temp;
						if(npcinfo[i].spriteset == 11) dq = 0;
						if(dq == 0) {
							movindown = 1;
							movinleft = 1;
						}
					}
					if(dq > 0) {
						sx = xp + ii;
						sy = yp + ii;
						temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
						dq = *temp;
						if(npcinfo[i].spriteset == 11) dq = 0;
						if(dq == 0 ) {
							movindown = 1;
							movinright = 1;
						}
					}
				}

				if(moveleft) {
					sx = xp - ii;
					sy = yp;
					temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
					dq = *temp;
					if(npcinfo[i].spriteset == 11) dq = 0;
					if(dq == 0) movinleft = 1;
					if(dq > 0) {
						sx = xp - ii;
						sy = yp - ii;
						temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
						dq = *temp;
						if(npcinfo[i].spriteset == 11) dq = 0;
						if(dq == 0) {
							movinleft = 1;
							movinup = 1;
						}
					}
					if(dq > 0) {
						sx = xp - ii;
						sy = yp + ii;
						temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
						dq = *temp;
						if(npcinfo[i].spriteset == 11) dq = 0;
						if(dq == 0) {
							movinleft = 1;
							movindown = 1;
						}
					}
				}

				if(moveright) {
					sx = xp + ii;
					sy = yp;
					temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
					dq = *temp;
					if(npcinfo[i].spriteset == 11) dq = 0;
					if(dq == 0) movinright = 1;
					if(dq > 0) {
						sx = xp + ii;
						sy = yp - ii;
						temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
						dq = *temp;
						if(npcinfo[i].spriteset == 11) dq = 0;
						if(dq == 0) {
							movinright = 1;
							movinup = 1;
						}
					}
					if(dq > 0) {
						sx = xp + ii;
						sy = yp + ii;
						temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
						dq = *temp;
						if(npcinfo[i].spriteset == 11) dq = 0;
						if(dq == 0) {
							movinright = 1;
							movindown = 1;
						}
					}
				}

				if(movinup) npy = npy - wspd * fpsr;
				if(movindown) npy = npy + wspd * fpsr;
				if(movinleft) npx = npx - wspd * fpsr;
				if(movinright) npx = npx + wspd * fpsr;

				if(checkpass == 1) {
					pass = 0;
					if(npx >= npcinfo[i].x1 * 16 - 8 && npx <= npcinfo[i].x2 * 16 + 8 && npy >= npcinfo[i].y1 * 16 - 8 && npy <= npcinfo[i].y2 * 16 + 8) pass = 1;
					if(pass == 0) {
						npx = onpx;
						npy = onpy;
						npcinfo[i].ticks = ticks;
					}
				}

				float aspd = wspd;

				if(npcinfo[i].spriteset == 10) aspd = wspd / 2;

				xp = (npx / 2 + 6);
				yp = (npy / 2 + 10);

				sx = xp;
				sy = yp;
				temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
				bgc = *temp;

				SDL_UnlockSurface(clipbg);

				float anpx = npx + 12;
				float anpy = npy + 20;

				int lx = (int)anpx / 16;
				int ly = (int)anpy / 16;

				if(triggerloc[lx][ly] > -1) bgc = 1;
				if(npcinfo[i].spriteset == 11) bgc = 0;

				int rst = 0;

				if(npcinfo[i].spriteset == 11) {
					if(npx < 40 || npx > 280 || npy < 36 || npy > 204) rst = 1;
				}

				if(bgc > 0 || rst == 1) {
					npx = onpx;
					npy = onpy;
				}

				npcinfo[i].x = npx;
				npcinfo[i].y = npy;

				npcinfo[i].walkdir = wdir;
				npcinfo[i].moving = 0;

				if(npx != onpx || npy != onpy) npcinfo[i].moving = 1;

				if(npcinfo[i].moving == 1) {
					float frame = npcinfo[i].frame;
					int cframe = npcinfo[i].cframe;

					frame = frame + aspd * fpsr;
					while(frame >= 16)
						frame = frame - 16;

					cframe = (int)(frame);
					if(cframe > 16) cframe = 16 - 1;
					if(cframe < 0) cframe = 0;

					npcinfo[i].frame = frame;
					npcinfo[i].cframe = cframe;
				}

				// spriteset1 specific
				if(npcinfo[i].spriteset == 1 && npcinfo[i].attackattempt < ticks) {
					if(npcinfo[i].attacknext < game_time && npcinfo[i].pause < game_time && npcinfo[i].attacking == 0) {
						npx = npcinfo[i].x;
						npy = npcinfo[i].y;

						xdif = player.px - npx;
						ydif = player.py - npy;

						if(abs(xdif) < 20 && abs(ydif) < 20) {
							npcinfo[i].attackattempt = ticks + 100;
							if((int)(RND() * 2) == 0) {
								game_playsound(sndenemyhit);

								npcinfo[i].attacking = 1;
								npcinfo[i].attackframe = 0;
							}
						}
					}
				}

				// onewing specific
				if(npcinfo[i].spriteset == 2) {
					if(npcinfo[i].attacknext < game_time && npcinfo[i].pause < game_time && npcinfo[i].attacking == 0) {
							npx = npcinfo[i].x;
							npy = npcinfo[i].y;

							xdif = player.px - npx;
							ydif = player.py - npy;

							if(abs(xdif) < 24 && abs(ydif) < 24) {
								float dist = sqrt(xdif * xdif + ydif * ydif);

								if((dist) < 24) {
									if(menabled == 1 && config.effects == 1) {
										int snd = Mix_PlayChannel(-1, sfx[sndbite], 0);
										Mix_Volume(snd, config.effectsvol);
									}

									npcinfo[i].attacking = 1;
									npcinfo[i].attackframe = 0;

									npcinfo[i].headtargetx[0] = player.px + 12;
									npcinfo[i].headtargety[0] = player.py - 4;
								}
							}

					}

					dospell = 0;

					if(npcinfo[i].attacking == 0 && npcinfo[i].castpause < game_time) {
						npcinfo[i].swayspd = npcinfo[i].swayspd + npcinfo[i].swayspd / 200 * fpsr;
						if(npcinfo[i].swayspd > 15) {
							dospell = 1;
							npcinfo[i].swayspd = 1;
						}

						// sway code
						npcinfo[i].swayangle = npcinfo[i].swayangle + npcinfo[i].swayspd * fpsr;
						if(npcinfo[i].swayangle >= 360) npcinfo[i].swayangle = npcinfo[i].swayangle - 360;

						npcinfo[i].headtargetx[0] = npcinfo[i].x + (24 - npcinfo[i].swayspd / 2) * sin(3.14159 / 180 * npcinfo[i].swayangle) + 12;
						npcinfo[i].headtargety[0] = npcinfo[i].y - 36 + 16 + 8 * sin(3.14159 * 2 / 180 * npcinfo[i].swayangle);
					}

					if(dospell == 1) {
						npcinfo[i].pause = game_time + 3000;
						npcinfo[i].attacknext = game_time + 4500;
						npcinfo[i].castpause = game_time + 4500;

						game_castspell(3, npcinfo[i].x, npcinfo[i].y, npcinfo[i].x, npcinfo[i].y, i);

						npcinfo[i].headtargetx[0] = npcinfo[i].x;
						npcinfo[i].headtargety[0] = npcinfo[i].y - 36 + 16;
					}

					// targethead code
					xdif = npcinfo[i].bodysection[7].x - npcinfo[i].headtargetx[0];
					ydif = npcinfo[i].bodysection[7].y - npcinfo[i].headtargety[0];


					npcinfo[i].bodysection[7].x = npcinfo[i].bodysection[7].x  - xdif * 0.4 * fpsr;
					npcinfo[i].bodysection[7].y = npcinfo[i].bodysection[7].y  - ydif * 0.4 * fpsr;


					npcinfo[i].bodysection[0].x = npcinfo[i].x + 12;
					npcinfo[i].bodysection[0].y = npcinfo[i].y + 12;

					for(int f = 6; f >= 1; f--) {
						xdif = npcinfo[i].bodysection[f + 1].x - npcinfo[i].bodysection[f - 1].x;
						ydif = npcinfo[i].bodysection[f + 1].y - npcinfo[i].bodysection[f - 1].y;

						float tx = npcinfo[i].bodysection[f - 1].x + xdif / 2;
						float ty = npcinfo[i].bodysection[f - 1].y + ydif / 2;

						npcinfo[i].bodysection[f].x = npcinfo[i].bodysection[f].x - (npcinfo[i].bodysection[f].x - tx) / 3;
						npcinfo[i].bodysection[f].y = npcinfo[i].bodysection[f].y - (npcinfo[i].bodysection[f].y - ty) / 3;
					}
				}

				// boss1 specific and blackknight
				if(npcinfo[i].spriteset == 3 || npcinfo[i].spriteset == 4) {
					if(npcinfo[i].attacknext < game_time && npcinfo[i].pause < game_time && npcinfo[i].attacking == 0) {
						npcinfo[i].attacking = 1;
						npcinfo[i].attackframe = 0;

						game_castspell(1, npcinfo[i].x, npcinfo[i].y, player.px, player.py, i);
					}

					if(npcinfo[i].castpause < game_time) {
						game_castspell(6, npcinfo[i].x, npcinfo[i].y, player.px, player.py, i);
						npcinfo[i].castpause = game_time + 12000;
					}
				}


				// firehydra specific
				if(npcinfo[i].spriteset == 5) {
					npcinfo[i].swayspd = 4;

					// sway code
					npcinfo[i].swayangle = npcinfo[i].swayangle + npcinfo[i].swayspd * fpsr;
					if(npcinfo[i].swayangle >= 360) npcinfo[i].swayangle = npcinfo[i].swayangle - 360;

					for(int ff = 0; ff <= 2; ff++) {
						if(npcinfo[i].hp > 10 * ff * 20) {
							if(npcinfo[i].pause < game_time && npcinfo[i].attacking2[ff] == 0 && npcinfo[i].attacknext2[ff] < game_time) {
								npx = npcinfo[i].x;
								npy = npcinfo[i].y;

								xdif = player.px - npx;
								ydif = player.py - npy;

								if(abs(xdif) < 48 && abs(ydif) < 48) {
									float dist = sqrt(xdif * xdif + ydif * ydif);

									if((dist) < 36) {
										game_playsound(sndbite);

										npcinfo[i].attacking = 1;
										npcinfo[i].attacking2[ff] = 1;
										npcinfo[i].attackframe2[ff] = 0;

										npcinfo[i].headtargetx[ff] = player.px + 12;
										npcinfo[i].headtargety[ff] = player.py - 4;

										npcinfo[i].swayangle = 0;
									}
								}

							}

							if(npcinfo[i].attacking2[ff] == 0) {
								npcinfo[i].headtargetx[ff] = npcinfo[i].x + 38 * sin(3.14159 / 180 * (npcinfo[i].swayangle + 120 * ff)) + 12;
								npcinfo[i].headtargety[ff] = npcinfo[i].y - 46 + 16 + 16 * sin(3.14159 * 2 / 180 * (npcinfo[i].swayangle + 120 * ff));
							}

							// targethead code
							xdif = npcinfo[i].bodysection[10 * ff + 9].x - npcinfo[i].headtargetx[ff];
							ydif = npcinfo[i].bodysection[10 * ff + 9].y - npcinfo[i].headtargety[ff];

							npcinfo[i].bodysection[10 * ff + 9].x = npcinfo[i].bodysection[10 * ff + 9].x  - xdif * 0.4 * fpsr;
							npcinfo[i].bodysection[10 * ff + 9].y = npcinfo[i].bodysection[10 * ff + 9].y  - ydif * 0.4 * fpsr;

							npcinfo[i].bodysection[10 * ff].x = npcinfo[i].x + 12 + 8 * cos(3.141592 * 2 * (itemyloc / 16 + ff * 120 / 360));
							npcinfo[i].bodysection[10 * ff].y = npcinfo[i].y + 12 + 8 * sin(3.141592 * 2 * (itemyloc / 16 + ff * 120 / 360));

							for(int f = 8; f >= 1; f--) {
								xdif = npcinfo[i].bodysection[ff * 10 + f + 1].x - npcinfo[i].bodysection[ff * 10 + f - 1].x;
								ydif = npcinfo[i].bodysection[ff * 10 + f + 1].y - npcinfo[i].bodysection[ff * 10 + f - 1].y;

								float tx = npcinfo[i].bodysection[ff * 10 + f - 1].x + xdif / 2;
								float ty = npcinfo[i].bodysection[ff * 10 + f - 1].y + ydif / 2;

								npcinfo[i].bodysection[ff * 10 + f].x = npcinfo[i].bodysection[ff * 10 + f].x - (npcinfo[i].bodysection[ff * 10 + f].x - tx) / 3;
								npcinfo[i].bodysection[ff * 10 + f].y = npcinfo[i].bodysection[ff * 10 + f].y - (npcinfo[i].bodysection[ff * 10 + f].y - ty) / 3;
							}
						}
					}
				}

				// spriteset6 specific
				if(npcinfo[i].spriteset == 6 && npcinfo[i].attackattempt < ticks) {
					if(npcinfo[i].attacknext < game_time && npcinfo[i].pause < game_time && npcinfo[i].attacking == 0) {
						npx = npcinfo[i].x;
						npy = npcinfo[i].y;

						xdif = player.px - npx;
						ydif = player.py - npy;

						pass = 0;
						if(abs(xdif) < 48 && abs(ydif) < 6) pass = 1;
						if(abs(ydif) < 48 && abs(xdif) < 6) pass = 2;

						if(pass > 0) {
							npcinfo[i].attackattempt = ticks + 100;
							if((int)(RND() * 2) == 0) {
								npcinfo[i].attacking = 1;
								npcinfo[i].attackframe = 0;

								if(pass == 1 && xdif < 0) {
									nnxa = npx - 8;
									nnya = npy + 4;
									nnxb = npx - 48 - 8;
									nnyb = npy + 4;
								} else if(pass == 1 && xdif > 0) {
									nnxa = npx + 16;
									nnya = npy + 4;
									nnxb = npx + 16 + 48;
									nnyb = npy + 4;
								} else if(pass == 2 && ydif < 0) {
									nnya = npy;
									nnxa = npx + 4;
									nnyb = npy - 48;
									nnxb = npx + 4;
								} else if(pass == 2 && ydif > 0) {
									nnya = npy + 20;
									nnxa = npx + 4;
									nnyb = npy + 20 + 48;
									nnxb = npx + 4;
								}

								game_castspell(7, nnxa, nnya, nnxb, nnyb, i);
							}
						}
					}
				}

				// wizard1 specific
				if(npcinfo[i].spriteset == 7) {
					if(npcinfo[i].attacknext < game_time && npcinfo[i].pause < game_time && npcinfo[i].attacking == 0) {
						npcinfo[i].attacking = 1;
						npcinfo[i].attackframe = 0;

						game_castspell(9, npcinfo[i].x, npcinfo[i].y, player.px, player.py, i);
					}

					if(npcinfo[i].castpause < game_time) {
						// game_castspell 6, npcinfo[i].x, npcinfo[i].y, player.px, player.py, i
						// npcinfo[i].castpause = game_time + 12000
					}

				}

				// spriteset6 specific
				if(npcinfo[i].spriteset == 8 && npcinfo[i].attackattempt < ticks) {
					if(npcinfo[i].attacknext < game_time && npcinfo[i].pause < game_time && npcinfo[i].attacking == 0) {
						npx = npcinfo[i].x;
						npy = npcinfo[i].y;

						xdif = player.px - npx;
						ydif = player.py - npy;

						pass = 0;
						if(abs(xdif) < 56 && abs(ydif) < 6) pass = 1;
						if(abs(ydif) < 56 && abs(xdif) < 6) pass = 2;

						if(pass > 0) {
							npcinfo[i].attackattempt = ticks + 100;
							if((int)(RND() * 2) == 0) {
								npcinfo[i].attacking = 1;
								npcinfo[i].attackframe = 0;

								if(pass == 1 && xdif < 0) {
									nnxa = npx - 8;
									nnya = npy + 4;
									nnxb = npx - 56 - 8;
									nnyb = npy + 4;
									npcinfo[i].walkdir = 2;
								} else if(pass == 1 && xdif > 0) {
									nnxa = npx + 16;
									nnya = npy + 4;
									nnxb = npx + 16 + 56;
									nnyb = npy + 4;
									npcinfo[i].walkdir = 3;
								} else if(pass == 2 && ydif < 0) {
									nnya = npy;
									nnxa = npx + 4;
									nnyb = npy - 56;
									nnxb = npx + 4;
									npcinfo[i].walkdir = 0;
								} else if(pass == 2 && ydif > 0) {
									nnya = npy + 20;
									nnxa = npx + 4;
									nnyb = npy + 20 + 56;
									nnxb = npx + 4;
									npcinfo[i].walkdir = 1;
								}

								game_castspell(7, nnxa, nnya, nnxb, nnyb, i);
							}
						}
					}
				}

				// twowing specific
				if(npcinfo[i].spriteset == 9) {
					if(npcinfo[i].attacknext < game_time && npcinfo[i].pause < game_time && npcinfo[i].attacking == 0) {
							npx = npcinfo[i].bodysection[7].x;
							npy = npcinfo[i].bodysection[7].y;

							xdif = player.px - npx;
							ydif = player.py - npy;

							if(abs(xdif) < 24 && abs(ydif) < 24) {
								float dist = sqrt(xdif * xdif + ydif * ydif);

								if((dist) < 24) {
									if(menabled == 1 && config.effects == 1) {
										int snd = Mix_PlayChannel(-1, sfx[sndbite], 0);
										Mix_Volume(snd, config.effectsvol);
									}

									npcinfo[i].attacking = 1;
									npcinfo[i].attackframe = 0;

									npcinfo[i].headtargetx[0] = player.px + 12;
									npcinfo[i].headtargety[0] = player.py - 4;
								}
							}

					}

					if(npcinfo[i].attacking == 0 && npcinfo[i].castpause < game_time) {
						npcinfo[i].swayspd = 4;

						// sway code
						npcinfo[i].swayangle = npcinfo[i].swayangle + npcinfo[i].swayspd * fpsr;
						if(npcinfo[i].swayangle >= 360) npcinfo[i].swayangle = npcinfo[i].swayangle - 360;

						npcinfo[i].headtargetx[0] = npcinfo[i].x + (24 - npcinfo[i].swayspd / 2) * sin(3.14159 / 180 * npcinfo[i].swayangle) + 12;
						npcinfo[i].headtargety[0] = npcinfo[i].y - 36 + 16 + 8 * sin(3.14159 * 2 / 180 * npcinfo[i].swayangle);
					}

					if(dospell == 1) {
						npcinfo[i].pause = game_time + 3000;
						npcinfo[i].attacknext = game_time + 5000;
						npcinfo[i].castpause = game_time + 3000;

						game_castspell(3, npcinfo[i].x, npcinfo[i].y, npcinfo[i].x, npcinfo[i].y, i);

						npcinfo[i].headtargetx[0] = npcinfo[i].x;
						npcinfo[i].headtargety[0] = npcinfo[i].y - 36 + 16;
					}

					// targethead code
					xdif = npcinfo[i].bodysection[7].x - npcinfo[i].headtargetx[0];
					ydif = npcinfo[i].bodysection[7].y - npcinfo[i].headtargety[0];


					npcinfo[i].bodysection[7].x = npcinfo[i].bodysection[7].x  - xdif * 0.4 * fpsr;
					npcinfo[i].bodysection[7].y = npcinfo[i].bodysection[7].y  - ydif * 0.4 * fpsr;

					npcinfo[i].bodysection[0].x = npcinfo[i].x + 12;
					npcinfo[i].bodysection[0].y = npcinfo[i].y + 12;

					for(int f = 6; f >= 1; f--) {
						xdif = npcinfo[i].bodysection[f + 1].x - npcinfo[i].bodysection[f - 1].x;
						ydif = npcinfo[i].bodysection[f + 1].y - npcinfo[i].bodysection[f - 1].y;

						float tx = npcinfo[i].bodysection[f - 1].x + xdif / 2;
						float ty = npcinfo[i].bodysection[f - 1].y + ydif / 2;

						npcinfo[i].bodysection[f].x = npcinfo[i].bodysection[f].x - (npcinfo[i].bodysection[f].x - tx) / 3;
						npcinfo[i].bodysection[f].y = npcinfo[i].bodysection[f].y - (npcinfo[i].bodysection[f].y - ty) / 3;
					}

				}

			}

			int dist; float damage;

			npx = npcinfo[i].x;
			npy = npcinfo[i].y;
			float xp = npx / 2 + 6;
			float yp = npy / 2 + 10;

			rcSrc.x = xp - 1;
			rcSrc.y = yp - 1;
			rcSrc.w = 3;
			rcSrc.h = 3;

			if(npcinfo[i].pause < game_time) SDL_FillRect(clipbg, &rcSrc, i);


			pass = 0;
			if(npcinfo[i].attacking == 1) pass = 1;
			if(npcinfo[i].spriteset == 5) {
				if(npcinfo[i].attacking2[0] == 1) pass = 1;
				if(npcinfo[i].attacking2[1] == 1) pass = 1;
				if(npcinfo[i].attacking2[2] == 1) pass = 1;
			}

			if(pass == 1) {
				// spriteset1 specific
				if(npcinfo[i].spriteset == 1) {
					npcinfo[i].attackframe = npcinfo[i].attackframe + npcinfo[i].attackspd * fpsr;
					if(npcinfo[i].attackframe >= 16) {
						npcinfo[i].attackframe = 0;
						npcinfo[i].attacking = 0;
						npcinfo[i].attacknext = game_time + npcinfo[i].attackdelay;
					}

					npcinfo[i].cattackframe = (int)(npcinfo[i].attackframe);

					npx = npcinfo[i].x;
					npy = npcinfo[i].y;

					xdif = player.px - npx;
					ydif = player.py - npy;

					dist = 10;

					if(abs(xdif) < dist && abs(ydif) < dist && player.pause < game_time) {
						npcinfo[i].attacknext = game_time + npcinfo[i].attackdelay;
						// npcinfo[i].attackframe = 0;
						// npcinfo[i].attacking = 0;

						damage = (float)npcinfo[i].attackdamage * (0.5 + RND() * 1.0);

						if(player.hp > 0) game_damageplayer(damage);
					}
				}

				if(npcinfo[i].spriteset == 2) npcserpentattack(&npcinfo[i]);

				// firehydra
				if(npcinfo[i].spriteset == 5) {
					for(int ff = 0; ff <= 2; ff++) {
						if(npcinfo[i].attacking2[ff] == 1) {
							xdif = npcinfo[i].bodysection[10 * ff + 9].x - npcinfo[i].headtargetx[ff];
							ydif = npcinfo[i].bodysection[10 * ff + 9].y - npcinfo[i].headtargety[ff];

							npcinfo[i].bodysection[10 * ff + 9].x = npcinfo[i].bodysection[10 * ff + 9].x  - xdif * .2 * fpsr;
							npcinfo[i].bodysection[10 * ff + 9].y = npcinfo[i].bodysection[10 * ff + 9].y  - ydif * .2 * fpsr;

							npcinfo[i].bodysection[10 * ff].x = npcinfo[i].x + 12 + 8 * cos(3.141592 * 2 * (itemyloc / 16 + ff * 120 / 360));
							npcinfo[i].bodysection[10 * ff].y = npcinfo[i].y + 12 + 8 * sin(3.141592 * 2 * (itemyloc / 16 + ff * 120 / 360));

							for(int f = 8; f >= 1; f--) {
								xdif = npcinfo[i].bodysection[ff * 10 + f + 1].x - npcinfo[i].bodysection[ff * 10 + f - 1].x;
								ydif = npcinfo[i].bodysection[ff * 10 + f + 1].y - npcinfo[i].bodysection[ff * 10 + f - 1].y;

								float tx = npcinfo[i].bodysection[ff * 10 + f - 1].x + xdif / 2;
								float ty = npcinfo[i].bodysection[ff * 10 + f - 1].y + ydif / 2;

								npcinfo[i].bodysection[ff * 10 + f].x = npcinfo[i].bodysection[ff * 10 + f].x - (npcinfo[i].bodysection[ff * 10 + f].x - tx) / 3;
								npcinfo[i].bodysection[ff * 10 + f].y = npcinfo[i].bodysection[ff * 10 + f].y - (npcinfo[i].bodysection[ff * 10 + f].y - ty) / 3;
							}

							npcinfo[i].attackframe2[ff] = npcinfo[i].attackframe2[ff] + npcinfo[i].attackspd * fpsr;
							if(npcinfo[i].attackframe2[ff] >= 16) {
								npcinfo[i].attackframe2[ff] = 0;
								npcinfo[i].attacking2[ff] = 0;
								npcinfo[i].attacknext2[ff] = game_time + npcinfo[i].attackdelay;
							}

							npcinfo[i].cattackframe = (int)(npcinfo[i].attackframe);

							npx = npcinfo[i].bodysection[10 * ff + 9].x;
							npy = (npcinfo[i].bodysection[10 * ff + 9].y + 16);

							xdif = (player.px + 12) - npx;
							ydif = (player.py + 12) - npy;

							dist = 8;

							if(abs(xdif) < dist && abs(ydif) < dist && player.pause < game_time) {
								npcinfo[i].attacknext2[ff] = game_time + npcinfo[i].attackdelay;
								// npcinfo[i].attackframe2(ff) = 0
								// npcinfo[i].attacking2(ff) = 0
								damage = (float)npcinfo[i].attackdamage * (1.0 + RND() * 0.5);
								if(player.hp > 0) game_damageplayer(damage);
							}
						}
					}

				}

				if(npcinfo[i].spriteset == 9) npcserpentattack(&npcinfo[i]);

				// dragon 2 specific
				if(npcinfo[i].spriteset == 10) {

					npcinfo[i].attackframe = npcinfo[i].attackframe + npcinfo[i].attackspd * fpsr;
					if(npcinfo[i].attackframe >= 16) {
						npcinfo[i].attackframe = 0;
						npcinfo[i].attacking = 0;
						npcinfo[i].attacknext = game_time + npcinfo[i].attackdelay;
					}

					npcinfo[i].cattackframe = (int)(npcinfo[i].attackframe);

					npx = npcinfo[i].x;
					npy = npcinfo[i].y;

					xdif = player.px - npx;
					ydif = player.py - npy;

					dist = 16 + npcinfo[i].attackframe;

					if(abs(xdif) < dist && abs(ydif) < dist && player.pause < game_time) {
						npcinfo[i].attacknext = game_time + npcinfo[i].attackdelay;
						// npcinfo[i].attackframe = 0
						// npcinfo[i].attacking = 0
						damage = (float)npcinfo[i].attackdamage * (0.5 + RND() * 1.0);
						if(player.hp > 0) game_damageplayer(damage);
					}
				}

				// endboss specific
				if(npcinfo[i].spriteset == 11) {
					npcinfo[i].attackframe = npcinfo[i].attackframe + npcinfo[i].attackspd * fpsr;
					if(npcinfo[i].attackframe >= 16) {
						npcinfo[i].attackframe = 0;
						npcinfo[i].attacking = 0;
						npcinfo[i].attacknext = game_time + npcinfo[i].attackdelay;
					}

					npcinfo[i].cattackframe = (int)(npcinfo[i].attackframe);

					npx = npcinfo[i].x;
					npy = npcinfo[i].y;

					xdif = player.px - npx;
					ydif = player.py - npy;

					dist = 36;

					if(abs(xdif) < dist && abs(ydif) < dist && player.pause < game_time) {
						npcinfo[i].attacknext = game_time + npcinfo[i].attackdelay;
						// npcinfo[i].attackframe = 0
						// npcinfo[i].attacking = 0
						damage = (float)npcinfo[i].attackdamage * (0.5 + RND() * 1.0);
						if(player.hp > 0) game_damageplayer(damage);
					}
				}
			}
			// -------end fight code
		}
		if(ai_target == 0) realpause = player.pause;
	}

	player.px = realpx;
	player.py = realpy;
	player.pause = realpause;
	ai_target = 0;
}

