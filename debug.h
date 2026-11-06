#ifndef DEBUG_H
#define DEBUG_H

extern int cheat_god, cheat_noclip;
extern int controlled_npc, monster_target, npc_infight, ai_target;

int debug_handlekey(int keycode);
void debug_keyup(int keycode);
void debug_resetcheats(void);
void debug_npctarget(int step);
void debug_controlledattack(int special);
void debug_handlecontrollednpc(void);
void debug_drawhud(void);

#endif
