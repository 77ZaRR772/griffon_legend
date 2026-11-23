#ifndef ITEMS_H
#define ITEMS_H

int items_allenemiesdead(void);
void items_drop(int npcnum, int obj_id, int mult);
void items_spawnchest(int cx, int cy, int obj_id, int flag_idx);
void items_placeobject(int cx, int cy, int object);

#endif
