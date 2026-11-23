#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL_mixer.h>

#include "griffon.h"
#include "config.h"
#include "debug.h"
#include "spells.h"

#undef abs
#define abs(x) ((x) < 0 ? -(x) : (x))

extern SDL_Surface *videobuffer, *clipbg, *clipbg2, *spellimg;
extern SDL_Rect rcSrc, rcDest;
extern SPELLTYPE spellinfo[MAXSPELL];
extern NPCTYPE npcinfo[MAXNPC];
extern int lastnpc, curmap, game_time, ticks, forcepause;
extern int objmap[21][15], objectinfo[33][6], nposts;
extern int tileinfo[3][40][24][3];
extern int elementmap[15][20];
extern float postinfo[21][3], fpsr, itemyloc;
extern int menabled;
extern Mix_Chunk *sfx[21];

static float spells_rnd(void)
{
	return (float)rand() * (1.0 / ((float)RAND_MAX + 1.0));
}
#define RND() spells_rnd()

void game_castspell(int spellnum, float homex, float homey, float enemyx, float enemyy, int damagewho)
{
	// spellnum 7 = sprite 6 spitfire

	for(int i = 0; i < MAXSPELL; i++) {
		if(spellinfo[i].frame == 0) {
			spellinfo[i].homex = homex;
			spellinfo[i].homey = homey;
			spellinfo[i].enemyx = enemyx;
			spellinfo[i].enemyy = enemyy;
			spellinfo[i].spellnum = spellnum;
			int dw = 0;
			int npc = 0;
			if(damagewho > 0) {
				dw = 1;
				npc = damagewho;
			}

			spellinfo[i].damagewho = dw;
			spellinfo[i].npc = npc;
			spellinfo[i].targetnpc = ai_target;
			memset(spellinfo[i].hitnext, 0, sizeof(spellinfo[i].hitnext));

			spellinfo[i].frame = 32;
			if(damagewho == 0) {
				spellinfo[i].strength = controlled_npc > 0 ? 1 : player.spellstrength / 100;
				if(controlled_npc == 0 && player.spellstrength == 100) spellinfo[i].strength = 1.5;
			}

			// set earthslide vars
			if(spellnum == 2) {
				for(int f = 0; f <= 8; f++) {
					spellinfo[i].rocky[f] = 0;
					spellinfo[i].rockimg[f] = (int)(RND() * 4);
					spellinfo[i].rockdeflect[f] = ((int)(RND() * 128) - 64) * 1.5;
				}
			}

			// set fire vars
			if(spellnum == 3) {
				for(int f = 0; f <= 4; f++) {
					spellinfo[i].legalive[f] = 32;
				}
			}

			if(spellnum == 7 && ai_target > 0) {
				float dx = npcinfo[ai_target].x - homex;
				float dy = npcinfo[ai_target].y - homey;
				if(dx * dx + dy * dy < 24 * 24)
					game_damagenpc(ai_target, npcinfo[damagewho].spelldamage, 1);
			}

			// room fireball vars
			if(spellnum == 6) {
				int nballs = 0;
				for(int x = 0; x <= 19; x++) {
					for(int y = 0; y <= 14; y++) {
						if((objmap[x][y] == 1 || objmap[x][y] == 2) && nballs < 5 && (int)(RND() * 4) == 0) {
							int ax = x * 16;
							int ay = y * 16;

							spellinfo[i].fireballs[nballs][0] = ax;
							spellinfo[i].fireballs[nballs][1] = ay;
							spellinfo[i].fireballs[nballs][2] = 0;
							spellinfo[i].fireballs[nballs][3] = 0;

							spellinfo[i].ballon[nballs] = 1;
							nballs = nballs + 1;
						}
					}
				}
				spellinfo[i].nfballs = nballs;
			}

			if(menabled == 1 && config.effects == 1) {
				if(spellnum == 1) {
					int snd = Mix_PlayChannel(-1, sfx[sndthrow], 0);
					Mix_Volume(snd, config.effectsvol);
				} else if(spellnum == 5) {
					int snd = Mix_PlayChannel(-1, sfx[sndcrystal], 0);
					Mix_Volume(snd, config.effectsvol);
				} else if(spellnum == 8 || spellnum == 9) {
					int snd = Mix_PlayChannel(-1, sfx[sndlightning], 0);
					Mix_Volume(snd, config.effectsvol);
				}
			}

			return;
		}
	}
}

int spells_lightningactive(void)
{
	for(int i = 0; i < MAXSPELL; i++) {
		if(spellinfo[i].frame > 0 && spellinfo[i].spellnum == 8) return 1;
	}
	return 0;
}

static void game_spellhit(int spellslot, int npcnum, float damage)
{
	if(npcnum == controlled_npc || npcinfo[npcnum].hp <= 0 || spellinfo[spellslot].hitnext[npcnum] > game_time) return;
	spellinfo[spellslot].hitnext[npcnum] = game_time + 900;

	damage += npcinfo[npcnum].spelldamagecarry;
	int wholedamage = (int)damage;
	npcinfo[npcnum].spelldamagecarry = damage - wholedamage;
	if(wholedamage > 0) game_damagenpc(npcnum, wholedamage, 1);
}

static void game_damageposts(float x, float y, int radius, int sound)
{
	for(int e = 0; e < nposts; e++) {
		int xdif = x - (postinfo[e][0] + 8);
		int ydif = y - (postinfo[e][1] + 8);
		if(abs(xdif) >= radius || abs(ydif) >= radius) continue;

		int px = (int)postinfo[e][0] / 16;
		int py = (int)postinfo[e][1] / 16;
		objmapf[curmap][px][py] = 1;
		objmap[px][py] = -1;
		rcSrc.x = postinfo[e][0] / 2;
		rcSrc.y = postinfo[e][1] / 2;
		rcSrc.w = 8;
		rcSrc.h = 8;
		SDL_FillRect(clipbg2, &rcSrc, 0);
		game_addFloatIcon(99, postinfo[e][0], postinfo[e][1]);
		if(sound >= 0) game_playsound(sound);
	}
}

static void game_lightningsegment(int slot, int x, int y, int horizontal,
	unsigned int cl1, unsigned int cl3)
{
	if(horizontal) {
		sys_line(videobuffer, x, y - 1, x, y + 2, cl1);
		sys_line(videobuffer, x, y, x, y + 1, cl3);
	} else {
		sys_line(videobuffer, x - 1, y, x + 2, y, cl1);
		sys_line(videobuffer, x, y, x + 1, y, cl3);
	}

	if(spellinfo[slot].damagewho == 0) {
		for(int e = 1; e <= lastnpc; e++) {
			int xdif = (x + 16) - (npcinfo[e].x + 12);
			int ydif = (y + 16) - (npcinfo[e].y + 12);
			if(abs(xdif) < 8 && abs(ydif) < 8) {
				float damage = 30 * (1 + RND() * 0.5);
				if(npcinfo[e].hp > 0) game_spellhit(slot, e, damage);
			}
		}
	}

	game_damageposts(x + 16, y + 16, 16, -1);
}

void game_updspells()
{
	int foundel[5];
	float npx, npy;
	long cl1, cl2, cl3;
	int ll[4][2];

	float xloc = 0, yloc = 0, xst, yst, xdif, ydif;
	float realpx = player.px, realpy = player.py;
	int realpause = player.pause;

	for(int i = 0; i < MAXSPELL; i++) {
		ai_target = spellinfo[i].targetnpc;
		player.px = realpx;
		player.py = realpy;
		player.pause = realpause;
		if(ai_target > 0) {
			player.px = npcinfo[ai_target].x;
			player.py = npcinfo[ai_target].y;
			player.pause = npcinfo[ai_target].pause;
		}
		if(spellinfo[i].frame > 0) {
			int spellnum = spellinfo[i].spellnum;

			// water
			if(spellnum == 0 && forcepause == 0) {
				float fr = (32 - spellinfo[i].frame);

				ll[0][0] = -2;
				ll[0][1] = -3;
				ll[1][0] = 2;
				ll[1][1] = -3;
				ll[2][0] = -4;
				ll[2][1] = -2;
				ll[3][0] = 4;
				ll[3][1] = -2;

				for(int f = 0; f <= 3; f++) {
					if(fr > f * 4 && fr < f * 4 + 16) {
						float alf = 255;

						if(fr < f * 4 + 8) {
							int fi = (int)((fr - f * 4) * 3) % 4;
							rcSrc.x = 32 + fi * 16;
							rcSrc.y = 80;
							rcSrc.w = 16;
							rcSrc.h = 16;

							xloc = spellinfo[i].enemyx + 12 + ll[f][0] * 16;
							yloc = spellinfo[i].enemyy + 16 + ll[f][1] * 16;

							rcDest.x = xloc;
							rcDest.y = yloc;

							alf = 255 * ((fr - f * 4) / 8);
						}

						if(fr >= f * 4 + 8) {
							int fi = 0; // ??

							if(f == 0 || f == 2) fi = 0;
							if(f == 1 || f == 3) fi = 1;
							rcSrc.x = 32 + fi * 16;
							rcSrc.y = 80;
							rcSrc.w = 16;
							rcSrc.h = 16;

							xst = spellinfo[i].enemyx + 12 + ll[f][0] * 16;
							yst = spellinfo[i].enemyy + 16 + ll[f][1] * 16;

							float xi = (spellinfo[i].enemyx - xst) * 2 / 8;
							float yi = (spellinfo[i].enemyy - yst) * 2 / 8;

							float fl = (fr - f * 4 - 8) / 2;
							xloc = xst + xi * fl * fl;
							yloc = yst + yi * fl * fl;

							rcDest.x = xloc;
							rcDest.y = yloc;

							alf = 255;
						}

						if(xloc > -16 && xloc < 304 && yloc > -16 && yloc < 224) {
							SDL_SetAlpha(spellimg, SDL_SRCALPHA, alf);
							SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);
							SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);

							if(spellinfo[i].damagewho == 0) {
								for(int e = 1; e <= lastnpc; e++) {

									xdif = (xloc + 16) - (npcinfo[e].x + 12);
									ydif = (yloc + 16) - (npcinfo[e].y + 12);

									if((abs(xdif) < 16 && abs(ydif) < 16)) {
										float basedamage = controlled_npc > 0 ? npcinfo[controlled_npc].spelldamage : player.spelldamage;
							float damage = basedamage * (1.0 + RND() * 0.5) * (float)spellinfo[i].strength;

										if(npcinfo[e].hp > 0) {
											game_spellhit(i, e, damage);
											game_playsound(sndice);
										}
									}
								}
							}

							// check for post damage
							game_damageposts(xloc + 16, yloc + 16, 16, sndice);
						}
					}
				}
			}

			// metal
			if(spellnum == 1 && forcepause == 0) {
				float xdif, ydif;
				int npc;
				int fr = (int)((32 - spellinfo[i].frame)*4) % 3;

				rcSrc.x = fr * 48;
				rcSrc.y = 0;
				rcSrc.w = 48;
				rcSrc.h = 48;

				float c1 = (32 - spellinfo[i].frame) / 16;

				float halfx = (spellinfo[i].homex-12) + ((spellinfo[i].enemyx-12) - (spellinfo[i].homex-12)) / 2;
				float halfy = (spellinfo[i].homey-12) + ((spellinfo[i].enemyy-12) - (spellinfo[i].homey-12)) / 2;

				float wdth = (halfx - spellinfo[i].homex) * 1.2;
				float hight = (halfy - spellinfo[i].homey) * 1.2;

				xloc = halfx + wdth * cos(3.14159 + 3.14159 * 2 * c1);
				yloc = halfy + hight * sin(3.14159 + 3.14159 * 2 * c1);

				rcDest.x = xloc;
				rcDest.y = yloc;

				SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);

				spellinfo[i].frame = spellinfo[i].frame - 0.2 * fpsr;
				if(spellinfo[i].frame < 0) spellinfo[i].frame = 0;

				if(spellinfo[i].damagewho == 0) {
					for(int e = 1; e <= lastnpc; e++) {
						xdif = (xloc + 24) - (npcinfo[e].x + 12);
						ydif = (yloc + 24) - (npcinfo[e].y + 12);

						if((abs(xdif) < 24 && abs(ydif) < 24)) {
							float basedamage = controlled_npc > 0 ? npcinfo[controlled_npc].spelldamage : player.spelldamage;
							float damage = basedamage * (1.0 + RND() * 0.5) * (float)spellinfo[i].strength;

							if(npcinfo[e].hp > 0) {
								game_spellhit(i, e, damage);
								game_playsound(sndmetalhit);
							}
						}
					}
				}

				if(spellinfo[i].damagewho == 1) {

					// --------- boss 1 specific
					if(spellinfo[i].frame == 0 && npcinfo[spellinfo[i].npc].spriteset == 3) {
						npc = spellinfo[i].npc;
						npcinfo[npc].attackframe = 0;
						npcinfo[npc].attacking = 0;

						npcinfo[npc].pause = game_time + 1000;
						npcinfo[npc].attacknext = game_time + 4000;
					}
					// ---------------

					// --------- blackknight specific
					if(spellinfo[i].frame == 0 && npcinfo[spellinfo[i].npc].spriteset == 4) {
						npc = spellinfo[i].npc;
						npcinfo[npc].attackframe = 0;
						npcinfo[npc].attacking = 0;

						npcinfo[npc].pause = game_time + 1000;
						npcinfo[npc].attacknext = game_time + 3500;
					}
					// ---------------

					xdif = (xloc + 24) - (player.px + 12);
					ydif = (yloc + 24) - (player.py + 12);

					if((abs(xdif) < 24 && abs(ydif) < 24) && player.pause < game_time) {
						npx = player.px;
						npy = player.py;

						float damage = (float)npcinfo[spellinfo[i].npc].spelldamage * (1.0 + RND() * 0.5);

						if(player.hp > 0) {
							game_damageplayer(damage);
							game_playsound(sndmetalhit);
						}
					}
				}


				// check for(int post damage
				game_damageposts(xloc + 24, yloc + 24, 24, sndmetalhit);
			}

			// earth
			if(spellnum == 2 && forcepause == 0) {
				float hght = 240 - spellinfo[i].enemyy;

				for(int f = 8; f >= 0; f--) {

					float fr = (32 - spellinfo[i].frame);

					if(fr > f && fr < f + 16) {
						rcSrc.x = 32 * spellinfo[i].rockimg[f];
						rcSrc.y = 48;
						rcSrc.w = 32;
						rcSrc.h = 32;

						int scatter = 0;
						if(fr < 8 + f) {
							xloc = spellinfo[i].enemyx - 4;
							yloc = spellinfo[i].enemyy * (1 - cos(3.14159 / 2 * (fr - f) / 8)); // ^ 2;
							yloc *= yloc;
						} else {
							scatter = 1;
							xloc = spellinfo[i].enemyx - 4 - spellinfo[i].rockdeflect[f] * sin(3.14159 / 2* ((fr - f) - 8) / 8);
							yloc = spellinfo[i].enemyy + hght * (1 - cos(3.14159 / 2 * ((fr - f) - 8) / 8));
						}

						rcDest.x = xloc;
						rcDest.y = yloc;

						if(xloc > -16 && xloc < 304 && yloc > -16 && yloc < 224) {
							SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);

							if(scatter == 1) {
								if(spellinfo[i].damagewho == 0) {
									for(int e = 1; e <= lastnpc; e++) {
										xdif = (xloc + 16) - (npcinfo[e].x + 12);
										ydif = (yloc + 16) - (npcinfo[e].y + 12);

										if((abs(xdif) < 16 && abs(ydif) < 16)) {
											float basedamage = controlled_npc > 0 ? npcinfo[controlled_npc].spelldamage : player.spelldamage;
							float damage = basedamage * (1.0 + RND() * 0.5) * (float)spellinfo[i].strength;

											if(npcinfo[e].hp > 0) {
												game_spellhit(i, e, damage);
												game_playsound(sndrocks);
											}
										}
									}
								}


								// check for(int post damage
								game_damageposts(xloc + 16, yloc + 16, 16, sndrocks);
							}
						}
					}
				}

				spellinfo[i].frame = spellinfo[i].frame - 0.2 * fpsr;
				if(spellinfo[i].frame < 0) spellinfo[i].frame = 0;
			}

			// crystal
			if(spellnum == 5) {

				float fra = (32 - spellinfo[i].frame);
				int fr = (int)((spellinfo[i].frame) * 2) % 8;

				rcSrc.x = fr * 32;
				rcSrc.y = 96 + 48;
				rcSrc.w = 32;
				rcSrc.h = 64;

				rcDest.x = player.px - 4;
				rcDest.y = player.py + 16 - 48;

				int f = 160;
				if(fra < 8) f = 192 * fra / 8;
				if(fra > 24) f = 192 * (1 - (fra - 24) / 8);

				SDL_SetAlpha(spellimg, SDL_SRCALPHA, f);
				SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);
				SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);

				spellinfo[i].frame = spellinfo[i].frame - 0.3 * fpsr;
				if(spellinfo[i].frame < 0) {
					spellinfo[i].frame = 0;
					game_setpause(0);

					npx = player.px + 12;
					npy = player.py + 20;

					int lx = (int)npx / 16;
					int ly = (int)npy / 16;

					for(int f = 0; f < 5; f++) { // !! f < 5
						foundel[f] = 0;
					}

					for(int xo = -2; xo <= 2; xo++) {
						for(int yo = -2; yo <= 2; yo++) {

							int sx = lx + xo;
							int sy = ly + yo;

							if(sx > -1 && sx < 20 && sy > -1 && sy < 15) {
								for(int l = 0; l <= 2; l++) {
									int curtile = tileinfo[l][sx][sy][0];
									int curtilel = tileinfo[l][sx][sy][1];

									if(curtile > 0) {
										curtile = curtile - 1;
										int curtilex = curtile % 20;
										int curtiley = (curtile - curtilex) / 20;

										int element = elementmap[curtiley][curtilex];
										if(element > -1 && curtilel == 0) foundel[element + 1] = 1;
									}
								}

								int o = objmap[sx][sy];
								if(o > -1) {
									if(objectinfo[o][4] == 1) foundel[2] = 1;
									if(o == 1 || o == 2) {
										foundel[2] = 1;
										foundel[4] = 1;
									}
								}
							}
						}
					}



					char line[256] = "";

					for(int f = 0; f < 5; f++) {
						if(foundel[f] == 1 && player.foundspell[f] == 0) {
							player.foundspell[f] = 1;
							player.spellcharge[f] = 0;

							if(f == 1) strcpy(line, "Found... Water Essence");
							else if(f == 2) strcpy(line, "Found... Metal Essence");
							else if(f == 3) strcpy(line, "Found... Earth Essence");
							else if(f == 4) strcpy(line, "Found... Fire Essence");
							break;
						}
					}

					if (line[0] != '\0') {
						game_eventtext(line);
					}

					int heal = player.maxhp / 2;
					int maxh = player.maxhp - player.hp;

					if(heal > maxh) heal = maxh;

					player.hp = player.hp + heal;

					char text[256];
					sprintf(text, "+%i", heal);
					game_addFloatText(text, player.px + 16 - 4 * strlen(text), player.py + 16, 5);

					game_playsound(sndpowerup);

				}
			}

			// room fireballs
			if(spellnum == 6 && forcepause == 0) {

				if(spellinfo[i].frame > 16) {
					float fr = (32 - spellinfo[i].frame);

					SDL_SetAlpha(spellimg, SDL_SRCALPHA, 192 * sin(3.14159 * fr / 4));

					rcSrc.x = 16 * (int)(RND() * 2);
					rcSrc.y = 80;
					rcSrc.w = 16;
					rcSrc.h = 16;

					for(int ff = 0; ff <= spellinfo[i].nfballs - 1; ff++) {

						xloc = spellinfo[i].fireballs[ff][0] + (int)(RND() * 3) - 1;
						yloc = spellinfo[i].fireballs[ff][1] + (int)(RND() * 3) - 1;

						rcDest.x = xloc;
						rcDest.y = yloc;

						SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);
					}

					SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);
				} else {
					SDL_SetAlpha(spellimg, SDL_SRCALPHA, 192);

					rcSrc.x = 16 * (int)(RND() * 2);
					rcSrc.y = 80;
					rcSrc.w = 16;
					rcSrc.h = 16;

					for(int ff = 0; ff <= spellinfo[i].nfballs - 1; ff++) {
						float ax = spellinfo[i].fireballs[ff][0];
						float ay = spellinfo[i].fireballs[ff][1];
						float bx = player.px + 4;
						float by = player.py + 4;
						float d = sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));

						float tx = (bx - ax) / d;
						float ty = (by - ay) / d;

						spellinfo[i].fireballs[ff][2] += tx * 1.2 * fpsr;
						spellinfo[i].fireballs[ff][3] += ty * 1.2 * fpsr;

						if(spellinfo[i].ballon[ff] == 1) {
							spellinfo[i].fireballs[ff][0] = ax + spellinfo[i].fireballs[ff][2] * 0.2 * fpsr;
							spellinfo[i].fireballs[ff][1] = ay + spellinfo[i].fireballs[ff][3] * 0.2 * fpsr;

							xloc = spellinfo[i].fireballs[ff][0] + (int)(RND() * 3) - 1;
							yloc = spellinfo[i].fireballs[ff][1] + (int)(RND() * 3) - 1;

							rcDest.x = xloc;
							rcDest.y = yloc;

							SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);
						}

						if(xloc < -1 || yloc < -1 || xloc > 304 || yloc > 224) spellinfo[i].ballon[ff] = 0;
					}

					SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);
				}

				spellinfo[i].frame = spellinfo[i].frame - 0.2 * fpsr;
				if(spellinfo[i].frame < 0) spellinfo[i].frame = 0;

				if(spellinfo[i].damagewho == 1) {
					for(int ff = 0; ff <= spellinfo[i].nfballs - 1; ff++) {
						if(spellinfo[i].ballon[ff] == 1) {
							xloc = spellinfo[i].fireballs[ff][0] + (int)(RND() * 3) - 1;
							yloc = spellinfo[i].fireballs[ff][1] + (int)(RND() * 3) - 1;

							xdif = (xloc + 8) - (player.px + 12);
							ydif = (yloc + 8) - (player.py + 12);

							if((abs(xdif) < 8 && abs(ydif) < 8) && player.pause < game_time) {
								float damage = npcinfo[spellinfo[i].npc].spelldamage * (1 + RND() * 0.5) / 3;

								if(player.hp > 0) game_damageplayer(damage);

								if(menabled == 1 && config.effects == 1) {
									 int snd = Mix_PlayChannel(-1, sfx[sndfire], 0);
									 Mix_Volume(snd, config.effectsvol);
								}
							}
						}
					}
				}
			}

			// lightning bomb
			if(spellnum == 8) {

				cl1 = SDL_MapRGB(videobuffer->format, 0, 32, 204);
				cl2 = SDL_MapRGB(videobuffer->format, 142, 173, 191);
				cl3 = SDL_MapRGB(videobuffer->format, 240, 240, 240);

				float px = player.px + 12;
				float py = player.py + 12;

				int apx = px + (int)(RND() * 5 - 2);
				int apy = py + (int)(RND() * 5 - 2);

				int directions[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
				for(int d = 0; d < 4; d++) {
					int x = apx;
					int y = apy;
					int orn = 0;
					int horizontal = directions[d][1] == 0;
					while(x >= 0 && x <= 319 && y >= 0 && y <= 239) {
						int rn = (int)(RND() * 3);
						if(horizontal) {
							if(orn == 0) y--;
							if(orn == 2) y++;
						} else {
							if(orn == 0) x--;
							if(orn == 2) x++;
						}
						orn = rn;
						game_lightningsegment(i, x, y, horizontal, cl1, cl3);
						if(horizontal) {
							if(rn == 0) sys_line(videobuffer, x, y + 1, x, y + 1, cl2);
							if(rn == 2) sys_line(videobuffer, x, y, x, y, cl2);
						} else {
							if(rn == 0) sys_line(videobuffer, x + 1, y, x + 1, y, cl2);
							if(rn == 2) sys_line(videobuffer, x, y, x, y, cl2);
						}
						x += directions[d][0];
						y += directions[d][1];
					}
				}

				spellinfo[i].frame -= 0.5 * fpsr;
				if(spellinfo[i].frame < 0) {
					spellinfo[i].frame = 0;
					game_setpause(0);
				}
			}

			// wizard 1 lightning
			if(spellnum == 9) {

				cl1 = SDL_MapRGB(videobuffer->format, 0, 32, 204);
				cl2 = SDL_MapRGB(videobuffer->format, 142, 173, 191);
				cl3 = SDL_MapRGB(videobuffer->format, 240, 240, 240);

				int px = spellinfo[i].enemyx + 12;
				int py = spellinfo[i].enemyy + 24;

				int apx = px + (int)(RND() * 20 - 10);
				int apy = py + (int)(RND() * 20 - 10);

				int x = apx;
				int orn = 0;
				for(int y = 0; y <= apy; y++) {
					if(y < 240) {
						float xdif, ydif;
						int rn = (int)(RND() * 3);

						if(orn == 0) x = x - 1;
						if(orn == 2) x = x + 1;

						sys_line(videobuffer,x - 1, y, x + 2, y, cl1);
						sys_line(videobuffer,x, y, x + 1, y, cl3);

						if(rn == 0) sys_line(videobuffer, x + 1, y, x + 1, y, cl2);
						if(rn == 2) sys_line(videobuffer, x, y, x, y, cl2);

						orn = rn;

						if(spellinfo[i].damagewho == 1) {

							xdif = (x + 8) - (player.px + 12);
							ydif = (y + 8) - (player.py + 12);

							if((abs(xdif) < 8 && abs(ydif) < 8) && player.pause < game_time) {
								float damage = ((float)player.hp * 0.75) * (RND() * 0.5 + 0.5);
								if(damage < 5) damage = 5;

								if(npcinfo[spellinfo[i].npc].spriteset == 12) {
									if(damage < 50) damage = 40 + (int)(RND() * 40);
								}

								if(player.hp > 0) game_damageplayer(damage);
							}
						} else {
							for(int e = 1; e <= lastnpc; e++) {
								if(e == controlled_npc) continue;
								xdif = (x + 8) - (npcinfo[e].x + 12);
								ydif = (y + 8) - (npcinfo[e].y + 12);
								if(abs(xdif) < 8 && abs(ydif) < 8 && npcinfo[e].hp > 0 && spellinfo[i].hitnext[e] <= game_time) {
									float damage = npcinfo[controlled_npc].spelldamage * (1.0 + RND() * 0.5);
									if(npcinfo[controlled_npc].spriteset == 12 && damage < 50) damage = 40 + RND() * 40;
									game_spellhit(i, e, damage);
									game_playsound(sndlightning);
								}
							}
						}
					}
				}

				spellinfo[i].frame -= 0.5 * fpsr;
				if(spellinfo[i].frame < 0) {
					spellinfo[i].frame = 0;

					npcinfo[spellinfo[i].npc].attacking = 0;
					npcinfo[spellinfo[i].npc].attacknext = game_time + npcinfo[spellinfo[i].npc].attackdelay;
				}
			}
			if(ai_target == 0) realpause = player.pause;
		}
	}
	player.px = realpx;
	player.py = realpy;
	player.pause = realpause;
	ai_target = 0;
}



void game_updspellsunder()
{

	unsigned int dq, *temp;

	if(forcepause == 1) return;

	for(int i = 0; i < MAXSPELL; i++) {
		ai_target = spellinfo[i].targetnpc;
		if(spellinfo[i].frame > 0) {
			int spellnum = spellinfo[i].spellnum;

			// water
			if(spellnum == 0) {
				int fra = (32 - spellinfo[i].frame);
				int fr = (int)((32 - spellinfo[i].frame) * 2) % 4;

				rcSrc.x = fr * 48;
				rcSrc.y = 96;
				rcSrc.w = 48;
				rcSrc.h = 48;

				rcDest.x = spellinfo[i].enemyx - 12;
				rcDest.y = spellinfo[i].enemyy - 8;

				int f = 160;
				if(fra < 8) f = 160 * fra / 8;
				if(fra > 24) f = 160 * (1 - (fra - 24) / 8);

				SDL_SetAlpha(spellimg, SDL_SRCALPHA, f);
				SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);
				SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);

				spellinfo[i].frame = spellinfo[i].frame - 0.2 * fpsr;
				if(spellinfo[i].frame < 0) spellinfo[i].frame = 0;


				for(int f = 1; f <= lastnpc; f++) {
					int xdif = spellinfo[i].enemyx - npcinfo[f].x;
					int ydif = spellinfo[i].enemyy - npcinfo[f].y;

					float dist = sqrt(xdif * xdif + ydif * ydif);

					if(dist > 20) dist = 20;

					if(dist > 5) {
						float ratio = (1 - dist / 25);

						float newx = npcinfo[f].x + ratio * xdif / 3 * fpsr;
						float newy = npcinfo[f].y + ratio * ydif / 3 * fpsr;

						int sx = (newx / 2 + 6);
						int sy = (newy / 2 + 10);
						temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
						dq = *temp;

						// SDL_UnLockSurface clipbg

						if(dq == 0) {
							npcinfo[f].x = newx;
							npcinfo[f].y = newy;
							// npcinfo[f].castpause = game_time + 200;
						} else {
							int xpass = 0;
							int ypass = 0;

							sx = (newx / 2 + 6);
							sy = (npcinfo[f].y / 2 + 10);
							temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
							dq = *temp;

							if(dq == 0) xpass = 1;


							sx = (npcinfo[f].x / 2 + 6);
							sy = (newy / 2 + 10);
							temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
							dq = *temp;

							if(dq == 0) ypass = 1;

							if(ypass == 1) {
								newx = npcinfo[f].x;
							} else if(xpass == 1) {
								newy = npcinfo[f].y;
							}

							if(xpass == 1 || ypass == 1) {
								npcinfo[f].x = newx;
								npcinfo[f].y = newy;
								// npcinfo[f].castpause = game_time + 200;
							}
						}
					}
				}
			}

			// fire
			if(spellnum == 3) {
				float fr = (32 - spellinfo[i].frame);

				fr = fr*fr * (1 - cos(3.14159 / 4 + 3.14159 / 4 * fr / 32));

				if(fr > 32) fr = 32;

				float s = 8;
				if(spellinfo[i].frame < 8) s = spellinfo[i].frame;

				int fra = (int)fr;

				for(int f = 0; f <= 4; f++) {
					for(int x = 0; x <= fra; x += 2) {
						if(spellinfo[i].legalive[f] >= x) {
							SDL_SetAlpha(spellimg, SDL_SRCALPHA, 192 * sin(3.14159 * x / 32) * s / 8);

							float an = 360 / 5 * f + x / 32 * 180;

							rcSrc.x = 16 * (int)(RND() * 2);
							rcSrc.y = 80;
							rcSrc.w = 16;
							rcSrc.h = 16;

							float xloc = (float)(spellinfo[i].enemyx + 4 + x * 2 * cos(3.14159 / 180 * an) + (int)(RND() * 3) - 1);
							float yloc = (float)(spellinfo[i].enemyy + 4 + x * 2 * sin(3.14159 / 180 * an) + (int)(RND() * 3) - 1);
							rcDest.x = (int)xloc;
							rcDest.y = (int)yloc;

							if(xloc > -1 && xloc < 304 && yloc > -1 && yloc < 224) {
								SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);

								int sx = (xloc / 2 + 4);
								int sy = (yloc / 2 + 8);
								temp = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
								dq = *temp;

								if(dq > 1000 && x > 4) spellinfo[i].legalive[f] = x;

								if(spellinfo[i].damagewho == 0) {
									for(int e = 1; e <= lastnpc; e++) {
										float xdif = (xloc + 8) - (npcinfo[e].x + 12);
										float ydif = (yloc + 8) - (npcinfo[e].y + 12);

										if((abs(xdif) < 8 && abs(ydif) < 8)) {
											float basedamage = controlled_npc > 0 ? npcinfo[controlled_npc].spelldamage : player.spelldamage;
							float damage = basedamage * (1.0 + RND() * 0.5) * (float)spellinfo[i].strength;

											if(npcinfo[e].spriteset == 5) damage = -damage;
											if(npcinfo[e].spriteset == 11) damage = -damage;
											if(npcinfo[e].hp > 0) {
												game_spellhit(i, e, damage);
												game_playsound(sndfire);
											}
										}
									}
								}

								if(spellinfo[i].damagewho == 1) {
									float xdif = (xloc + 8) - (player.px + 12);
									float ydif = (yloc + 8) - (player.py + 12);

									if((abs(xdif) < 8 && abs(ydif) < 8) && player.pause < game_time) {
										float damage = (float)npcinfo[spellinfo[i].npc].spelldamage * (1.0 + RND() * 0.5);

										if(player.hp > 0) {
											game_damageplayer(damage);

											game_playsound(sndfire);
										}
									}
								}

								// check for post damage
								if(nposts > 0) {
									for(int e = 0; e <= nposts - 1; e++) {
										float xdif = (xloc + 8) - (postinfo[e][0] + 8);
										float ydif = (yloc + 8) - (postinfo[e][1] + 8);

										if((abs(xdif) < 8 && abs(ydif) < 8)) {
											objmapf[curmap][(int)postinfo[e][0] / 16][(int)postinfo[e][1] / 16] = 1;
											objmap[(int)postinfo[e][0] / 16][(int)postinfo[e][1] / 16] = -1;

											rcSrc.x = postinfo[e][0] / 2;
											rcSrc.y = postinfo[e][1] / 2;
											rcSrc.w = 8;
											rcSrc.h = 8;

											SDL_FillRect(clipbg2, &rcSrc, 0);

											game_playsound(sndfire);

											game_addFloatIcon(99, postinfo[e][0], postinfo[e][1]);
										}
									}
								}
							}
						}
					}
				}

				SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);

				spellinfo[i].frame = spellinfo[i].frame - 0.2 * fpsr;
				if(spellinfo[i].frame < 0) spellinfo[i].frame = 0;


			}


			// sprite 6 spitfire
			if(spellnum == 7) {
				float xspan = spellinfo[i].enemyx - spellinfo[i].homex;
				float yspan = spellinfo[i].enemyy - spellinfo[i].homey;
				float fr = (32 - spellinfo[i].frame);

				for(int f = 0; f <= 7; f++) {
					int alpha = 0;
					float xx = 0;
					if(fr > f * 2 && fr < f * 2 + 16) xx = fr - f * 2;
					if(xx < 8) alpha = 255 * xx / 8;
					if(xx > 8) alpha = 255 * (1 - (xx - 8) / 8);
					float yy = 16 * sin(3.141592 / 2 * xx / 16) - 8;

					if(alpha < 0) alpha = 0;
					if(alpha > 255) alpha = 255;

					SDL_SetAlpha(spellimg, SDL_SRCALPHA, alpha);

					rcSrc.x = 16 * (int)(RND() * 2);
					rcSrc.y = 80;
					rcSrc.w = 16;
					rcSrc.h = 16;

					float xloc = spellinfo[i].homex + xspan / 7 * f;
					float yloc = spellinfo[i].homey + yspan / 7 * f - yy;

					rcDest.x = xloc;
					rcDest.y = yloc;

					if(xloc > -16 && xloc < 320 && yloc > -16 && yloc < 240) {
						SDL_BlitSurface(spellimg, &rcSrc, videobuffer, &rcDest);

						if(spellinfo[i].damagewho == 1) {
							float xdif = (xloc + 8) - (player.px + 12);
							float ydif = (yloc + 8) - (player.py + 12);

							if((abs(xdif) < 8 && abs(ydif) < 8) && player.pause < game_time && alpha > 64) {
								float damage = (float)npcinfo[spellinfo[i].npc].spelldamage * (1.0 + RND() * 0.5);

								if(player.hp > 0) {
									game_damageplayer(damage);
									game_playsound(sndfire);
								}

							}
						} else if(alpha > 64) {
							for(int e = 1; e <= lastnpc; e++) {
								if(e == controlled_npc) continue;
								float xdif = (xloc + 8) - (npcinfo[e].x + 12);
								float ydif = (yloc + 8) - (npcinfo[e].y + 12);
								if(abs(xdif) < 8 && abs(ydif) < 8 && npcinfo[e].hp > 0 && spellinfo[i].hitnext[e] <= game_time) {
									float damage = npcinfo[controlled_npc].spelldamage * (1.0 + RND() * 0.5);
									game_spellhit(i, e, damage);
									game_playsound(sndfire);
								}
							}
						}

					}

				}

				SDL_SetAlpha(spellimg, SDL_SRCALPHA, 255);
				spellinfo[i].frame = spellinfo[i].frame - 0.5 * fpsr;
				if(spellinfo[i].frame < 0) spellinfo[i].frame = 0;

				if(spellinfo[i].frame == 0) {
					npcinfo[spellinfo[i].npc].attacking = 0;
					npcinfo[spellinfo[i].npc].attacknext = game_time + npcinfo[spellinfo[i].npc].attackdelay;
				}
			}
		}
	}
}

