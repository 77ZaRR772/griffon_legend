#include <math.h>
#include <stdio.h>
#include <SDL/SDL_mixer.h>

#include "griffon.h"
#include "config.h"
#include "debug.h"
#include "editor.h"

int cheat_god, cheat_noclip;
int controlled_npc, monster_target, npc_infight, ai_target;
static int windowkey;

extern SDL_Surface *video, *videobuffer, *videobuffer2;
extern SDL_Surface *clipbg;
extern NPCTYPE npcinfo[MAXNPC];
extern int lastnpc, itemticks, ticks, game_time;
extern int selenemyon, curenemy;
extern int movingup, movingdown, movingleft, movingright;
extern int menabled;
extern float fps, fpsr, animspd;
extern Mix_Chunk *sfx[21];

void game_setpause(int paused);
void sdl_blitscale(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
int sys_pollevent(void);

static int debug_mapexists(int mapnum)
{
	char name[256];
	FILE *fp;

	sprintf(name, "mapdb/%04i.map", mapnum);
	fp = fopen(name, "r");
	if(!fp) return 0;
	fclose(fp);
	return 1;
}

static int debug_nextmap(int mapnum, int step)
{
	for(int i = 0; i <= 262; i++) {
		mapnum = (mapnum + step + 263) % 263;
		if(debug_mapexists(mapnum)) return mapnum;
	}
	return curmap;
}

static void debug_mapmenu(void)
{
	int selected = debug_mapexists(curmap) ? curmap : debug_nextmap(curmap, 1);
	int keypause = ticks + 50;
	int f12released = 0;
	char line[64];

	game_setpause(1);
	SDL_BlitSurface(videobuffer, NULL, videobuffer2, NULL);

	do {
		SDL_BlitSurface(videobuffer2, NULL, videobuffer, NULL);
		game_fillrect(videobuffer, 48, 80, 224, 80, 0);
		sys_print(videobuffer, "DEBUG: SELECT MAP", 100, 92, 3);
		sprintf(line, "%03i", selected);
		sys_print(videobuffer, line, 136, 112, 4);
		sys_print(videobuffer, "ARROWS: CHANGE", 96, 130, 0);
		sys_print(videobuffer, "CTRL/ENTER: LOAD", 96, 140, 0);
		sys_print(videobuffer, "ESC/F12: CLOSE", 96, 150, 0);
#ifdef OPENDINGUX
		SDL_BlitSurface(videobuffer, NULL, video, NULL);
#else
		sdl_blitscale(videobuffer, NULL, video, NULL);
#endif
		SDL_Flip(video);

		sys_pollevent();
		if(event.type == SDL_KEYUP && (event.key.keysym.sym == SDLK_F12 ||
			event.key.keysym.sym == (SDLKey)config.key_dbg)) f12released = 1;
		if(event.type == SDL_KEYDOWN && keypause < ticks) {
			int keycode = event.key.keysym.sym;
			keypause = ticks + 150;

			if(keycode == config.key_left || keycode == config.key_down || keycode == SDLK_PAGEDOWN)
				selected = debug_nextmap(selected, keycode == SDLK_PAGEDOWN ? -10 : -1);
			if(keycode == config.key_right || keycode == config.key_up || keycode == SDLK_PAGEUP)
				selected = debug_nextmap(selected, keycode == SDLK_PAGEUP ? 10 : 1);
			if(keycode == config.key_menu || (keycode == config.key_dbg && f12released)) break;
			if(keycode == config.key_attack || keycode == SDLK_LCTRL ||
				keycode == SDLK_RCTRL || keycode == SDLK_RETURN) {
				game_loadmap(selected);
				break;
			}
		}
		sys_tickcalc();
	} while(1);

	itemticks = ticks + 250;
	if(f12released) debug_keyup(SDLK_F12);
	game_setpause(0);
}

void debug_npctarget(int step)
{
	if(lastnpc == 0) {
		game_eventtext("No monsters on this map");
		return;
	}

	if(curenemy < 1 || curenemy > lastnpc || npcinfo[curenemy].hp <= 0) {
		curenemy = 0;
		step = 1;
	}
	int origin = curenemy;
	do {
		curenemy += step;
		if(curenemy < 1) curenemy = lastnpc;
		if(curenemy > lastnpc) curenemy = 1;
	} while(npcinfo[curenemy].hp <= 0 && curenemy != origin);

	if(npcinfo[curenemy].hp <= 0) {
		game_eventtext("No monsters on this map");
		return;
	}

	selenemyon = 1;
	monster_target = 1;
}

static void debug_bossbite(NPCTYPE *npc, NPCTYPE *target, int hitplayer)
{
	int head = 0;
	if(npc->spriteset == 5) {
		head = -1;
		for(int i = 0; i < 3; i++) {
			if(npc->hp > i * 200 && !npc->attacking2[i] && npc->attacknext2[i] < game_time) {
				head = i;
				break;
			}
		}
		if(head < 0) return;
		npc->attacking2[head] = 1;
		npc->attackframe2[head] = 0;
	} else {
		npc->attacking = 1;
		npc->attackframe = 0;
	}
	npc->headtargetx[head] = (hitplayer ? player.px : target->x) + 12;
	npc->headtargety[head] = (hitplayer ? player.py : target->y) - 4;

	if(menabled == 1 && config.effects == 1) {
		int sound = Mix_PlayChannel(-1, sfx[sndbite], 0);
		Mix_Volume(sound, config.effectsvol);
	}
}

void debug_controlledattack(int special)
{
	NPCTYPE *npc = &npcinfo[controlled_npc];
	if(special && npc->spriteset == 1) return;
	if((special ? npc->specialnext : npc->attacknext) > game_time) return;

	NPCTYPE *target = NULL;
	float best = special ? 120 : 48;
	for(int i = 1; i <= lastnpc; i++) {
		if(i == controlled_npc || npcinfo[i].hp <= 0) continue;
		float dx = npcinfo[i].x - npc->x;
		float dy = npcinfo[i].y - npc->y;
		float distance = sqrt(dx * dx + dy * dy);
		if(distance < best) {
			best = distance;
			target = &npcinfo[i];
		}
	}
	int hitplayer = 0;
	if(!target) {
		float dx = player.px - npc->x;
		float dy = player.py - npc->y;
		if(sqrt(dx * dx + dy * dy) < best) hitplayer = 1;
		else return;
	}

	int targetnum = target ? (int)(target - npcinfo) : 0;
	if(special && (npc->spriteset == 3 || npc->spriteset == 4)) {
		game_castspell(1, npc->x, npc->y,
			hitplayer ? player.px : target->x, hitplayer ? player.py : target->y,
			hitplayer ? controlled_npc : 0);
		npc->specialnext = game_time + npc->attackdelay * 2;
		return;
	}
	if(special && (npc->spriteset == 2 || npc->spriteset == 5 || npc->spriteset == 9)) {
		game_castspell(3, npc->x, npc->y, npc->x, npc->y, 0);
		npc->specialnext = game_time + npc->attackdelay * 2;
		return;
	}
	if(special && (npc->spriteset == 7 || npc->spriteset == 12)) {
		game_castspell(9, npc->x, npc->y,
			hitplayer ? player.px : target->x, hitplayer ? player.py : target->y,
			hitplayer ? controlled_npc : 0);
		npc->specialnext = game_time + npc->attackdelay * 2;
		return;
	}
	if(special && (npc->spriteset == 6 || npc->spriteset == 8)) {
		float targetx = npc->x;
		float targety = npc->y;
		if(npc->walkdir == 0) targety -= 112;
		if(npc->walkdir == 1) targety += 112;
		if(npc->walkdir == 2) targetx -= 112;
		if(npc->walkdir == 3) targetx += 112;
		game_castspell(7, npc->x, npc->y, targetx, targety, 0);
		npc->specialnext = game_time + npc->attackdelay * 2;
		return;
	}
	if(!special && (npc->spriteset == 2 || npc->spriteset == 5 || npc->spriteset == 9)) {
		debug_bossbite(npc, target, hitplayer);
		return;
	}

	int damage = special ? npc->spelldamage : npc->attackdamage;
	if(damage <= 0) damage = special ? npc->attackdamage * 2 : npc->spelldamage;
	if(damage <= 0) damage = 2;
	if(hitplayer) game_damageplayer(damage);
	else game_damagenpc(targetnum, damage, 1);
	if(npc->spriteset != 6 && npc->spriteset != 8) {
		npc->attacking = 1;
		npc->attackframe = 0;
		npc->cattackframe = 0;
	}
	if(special) npc->specialnext = game_time + npc->attackdelay * 2;
	else npc->attacknext = game_time + npc->attackdelay;

	if(menabled == 1 && config.effects == 1) {
		int sound = Mix_PlayChannel(-1, sfx[special ? sndlightning : sndswordhit], 0);
		Mix_Volume(sound, config.effectsvol);
	}
}

void debug_keyup(int keycode)
{
	if(keycode == windowkey) windowkey = 0;
}

void debug_resetcheats(void)
{
	cheat_god = 0;
	cheat_noclip = 0;
	npc_infight = 0;
}

int debug_handlekey(int keycode)
{
	static int f8pause, f9pause, windowpause;

	if(!config.showdbginfo) return 0;

	switch(keycode) {
	case SDLK_F7:
		if(itemticks < ticks && windowpause < ticks && windowkey == 0) {
			itemticks = ticks + 500;
			windowpause = ticks + 500;
			windowkey = keycode;
			editor_run();
		}
		return 1;
	case SDLK_F8:
		if(f8pause < ticks) {
			f8pause = ticks + 300;
			npc_infight = !npc_infight;
			game_addFloatText(npc_infight ? "INFIGHT ON" : "INFIGHT OFF", player.px, player.py, 3);
		}
		return 1;
	case SDLK_F9:
		if(f9pause < ticks) {
			f9pause = ticks + 300;
			itemticks = ticks + 250;
			if(controlled_npc > 0) {
				controlled_npc = 0;
				selenemyon = 0;
				monster_target = 0;
				game_addFloatText("PLAYER", player.px, player.py, 3);
			} else if(selenemyon) {
				selenemyon = 0;
				monster_target = 0;
			} else {
				debug_npctarget(1);
			}
		}
		return 1;
	case SDLK_F12:
		if(itemticks < ticks && windowpause < ticks && windowkey == 0) {
			itemticks = ticks + 500;
			windowpause = ticks + 500;
			windowkey = keycode;
			debug_mapmenu();
		}
		return 1;
	case SDLK_F11:
		if(itemticks < ticks) {
			itemticks = ticks + 300;
			cheat_god = !cheat_god;
			game_addFloatText(cheat_god ? "GOD ON" : "GOD OFF", player.px, player.py, 3);
		}
		return 1;
	case SDLK_F10:
		if(itemticks < ticks) {
			itemticks = ticks + 300;
			cheat_noclip = !cheat_noclip;
			game_addFloatText(cheat_noclip ? "NOCLIP ON" : "NOCLIP OFF", player.px, player.py, 3);
		}
		return 1;
	default:
		return 0;
	}
}

static int debug_bosshit(NPCTYPE *npc, int head)
{
	int section = npc->spriteset == 5 ? head * 10 + 9 : 7;
	float hx = npc->bodysection[section].x;
	float hy = npc->bodysection[section].y + 16;

	for(int i = 1; i <= lastnpc; i++) {
		if(i == controlled_npc || npcinfo[i].hp <= 0) continue;
		if(fabs(hx - (npcinfo[i].x + 12)) < 10 && fabs(hy - (npcinfo[i].y + 12)) < 10) {
			game_damagenpc(i, npc->attackdamage, 1);
			return 1;
		}
	}
	if(fabs(hx - (player.px + 12)) < 10 && fabs(hy - (player.py + 12)) < 10 && player.pause < game_time) {
		game_damageplayer(npc->attackdamage);
		return 1;
	}
	return 0;
}

static void debug_updatebossbody(NPCTYPE *npc)
{
	int heads = npc->spriteset == 5 ? 3 : 1;
	npc->swayspd = npc->spriteset == 5 ? 4 : 1;
	npc->swayangle += npc->swayspd * fpsr;
	if(npc->swayangle >= 360) npc->swayangle -= 360;

	for(int head = 0; head < heads; head++) {
		if(npc->spriteset == 5 && npc->hp <= head * 200) continue;
		int attacking = npc->spriteset == 5 ? npc->attacking2[head] : npc->attacking;
		int section = npc->spriteset == 5 ? head * 10 + 9 : 7;
		int base = npc->spriteset == 5 ? head * 10 : 0;

		if(!attacking) {
			float angle = npc->swayangle + head * 120;
			npc->headtargetx[head] = npc->x + 12 + (npc->spriteset == 5 ? 38 : 22) * sin(3.14159 / 180 * angle);
			npc->headtargety[head] = npc->y - 20 + (npc->spriteset == 5 ? 10 : 16) + (npc->spriteset == 5 ? 16 : 8) * sin(3.14159 * 2 / 180 * angle);
		}

		float follow = attacking ? 0.2 : 0.4;
		npc->bodysection[section].x -= (npc->bodysection[section].x - npc->headtargetx[head]) * follow * fpsr;
		npc->bodysection[section].y -= (npc->bodysection[section].y - npc->headtargety[head]) * follow * fpsr;
		npc->bodysection[base].x = npc->x + 12 + (npc->spriteset == 5 ? 8 * cos(3.14159 * 2 * (npc->swayangle / 360 + head / 3.0)) : 0);
		npc->bodysection[base].y = npc->y + 12 + (npc->spriteset == 5 ? 8 * sin(3.14159 * 2 * (npc->swayangle / 360 + head / 3.0)) : 0);

		for(int f = section - 1; f >= base + 1; f--) {
			float xdif = npc->bodysection[f + 1].x - npc->bodysection[f - 1].x;
			float ydif = npc->bodysection[f + 1].y - npc->bodysection[f - 1].y;
			npc->bodysection[f].x -= (npc->bodysection[f].x - (npc->bodysection[f - 1].x + xdif / 2)) / 3;
			npc->bodysection[f].y -= (npc->bodysection[f].y - (npc->bodysection[f - 1].y + ydif / 2)) / 3;
		}

		if(attacking) {
			if(npc->spriteset == 5) {
				npc->attackframe2[head] += npc->attackspd * fpsr;
				if(debug_bosshit(npc, head)) npc->attackframe2[head] = 16;
				if(npc->attackframe2[head] >= 16) {
					npc->attacking2[head] = 0;
					npc->attackframe2[head] = 0;
					npc->attacknext2[head] = game_time + npc->attackdelay;
					npc->attacknext = npc->attacknext2[head];
				}
			} else {
				npc->attackframe += npc->attackspd * fpsr;
				if(debug_bosshit(npc, head)) npc->attackframe = 16;
				if(npc->attackframe >= 16) {
					npc->attacking = 0;
					npc->attackframe = 0;
					npc->attacknext = game_time + npc->attackdelay;
				}
			}
		}
	}
}

void debug_handlecontrollednpc(void)
{
	if(controlled_npc < 1 || controlled_npc > lastnpc || npcinfo[controlled_npc].hp <= 0) {
		controlled_npc = 0;
		return;
	}

	NPCTYPE *npc = &npcinfo[controlled_npc];
	int boss = npc->spriteset == 2 || npc->spriteset == 5 || npc->spriteset == 9;
	if(npc->attacking && !boss) {
		npc->attackframe += npc->attackspd * fpsr;
		npc->cattackframe = (int)npc->attackframe;
		if(npc->attackframe >= 16) {
			npc->attacking = 0;
			npc->attackframe = 0;
			npc->cattackframe = 0;
		}
	}

	float dx = 0, dy = 0;
	float speed = npc->walkspd * fpsr;
	if(movingup) dy = -speed;
	if(movingdown) dy = speed;
	if(movingleft) dx = -speed;
	if(movingright) dx = speed;
	if(dx < 0) npc->walkdir = 2;
	if(dx > 0) npc->walkdir = 3;
	if(dy < 0) npc->walkdir = 0;
	if(dy > 0) npc->walkdir = 1;

	float nx = npc->x + dx;
	float ny = npc->y + dy;
	if(nx >= -8 && nx <= 295 && ny >= -8 && ny <= 215) {
		int sx = (int)(nx / 2 + 6);
		int sy = (int)(ny / 2 + 10);
		unsigned int *pixel = clipbg->pixels + sy * clipbg->pitch + sx * clipbg->format->BytesPerPixel;
		if(cheat_noclip || *pixel == 0 || *pixel == 1000) {
			npc->x = nx;
			npc->y = ny;
		}
	}
	if(dx != 0 || dy != 0) {
		npc->frame += animspd * fpsr;
		if(npc->frame >= 16) npc->frame -= 16;
		npc->cframe = (int)npc->frame;
	}
	if(boss) debug_updatebossbody(npc);
}

void debug_drawhud(void)
{
	char line[128];

	sprintf(line, "fps: %i map: %i exp: %i/%i ", (int)fps, curmap, player.exp, player.nextlevel);
	sys_print(videobuffer, line, 0, 0, 0);
	if(selenemyon || controlled_npc > 0) {
		sprintf(line, "target: %i controlled: %i", curenemy, controlled_npc);
		sys_print(videobuffer, line, 0, 10, 0);
	}
}
