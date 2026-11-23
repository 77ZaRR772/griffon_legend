#include <SDL/SDL.h>

#include "griffon.h"
#include "config.h"
#include "input.h"

extern int windowfocused;

static void sys_joykey(SDLKey sym, int pressed)
{
	Uint8 *keystate = SDL_GetKeyState(NULL);

	if(sym >= SDLK_LAST) return;
	keystate[sym] = pressed;
	event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.type = event.type;
	event.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.sym = sym;
}

int sys_pollevent(void)
{
	static SDL_Event eventqueue[64];
	static int eventpos, eventcount;

	if(eventpos == eventcount) {
		eventpos = 0;
		eventcount = 0;
		SDL_PumpEvents();
		while(eventcount < 64 && SDL_PollEvent(&eventqueue[eventcount])) eventcount++;
		if(eventcount == 0) return 0;
	}

	event = eventqueue[eventpos++];
	int result = 1;

	if(event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
		SDLKey sym = event.key.keysym.sym;
		if(sym < SDLK_LAST) SDL_GetKeyState(NULL)[sym] = event.type == SDL_KEYDOWN;
	}

	if(event.type == SDL_QUIT) exit(0);
	if(event.type == SDL_ACTIVEEVENT && (event.active.state & SDL_APPINPUTFOCUS))
		windowfocused = event.active.gain != 0;

	if(event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP) {
		int pressed = event.type == SDL_JOYBUTTONDOWN;
		SDLKey sym = SDLK_UNKNOWN;

		if(event.jbutton.button == 0) sym = config.key_attack;
		if(event.jbutton.button == 1) sym = config.key_item;
		if(event.jbutton.button == 2 || event.jbutton.button == 7) sym = SDLK_RETURN;
		if(event.jbutton.button == 6 || event.jbutton.button == 9) sym = config.key_menu;
		if(sym != SDLK_UNKNOWN) sys_joykey(sym, pressed);
	}

	if(event.type == SDL_JOYAXISMOTION && event.jaxis.axis < 2) {
		SDLKey negative = event.jaxis.axis == 0 ? config.key_left : config.key_up;
		SDLKey positive = event.jaxis.axis == 0 ? config.key_right : config.key_down;
		int direction = event.jaxis.value < -8000 ? -1 : event.jaxis.value > 8000 ? 1 : 0;

		sys_joykey(negative, direction == -1);
		sys_joykey(positive, direction == 1);
	}

	if(event.type == SDL_JOYHATMOTION) {
		sys_joykey(config.key_left, event.jhat.value & SDL_HAT_LEFT);
		sys_joykey(config.key_right, event.jhat.value & SDL_HAT_RIGHT);
		sys_joykey(config.key_up, event.jhat.value & SDL_HAT_UP);
		sys_joykey(config.key_down, event.jhat.value & SDL_HAT_DOWN);
	}

	return result;
}

