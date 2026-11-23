#ifndef MODS_H
#define MODS_H

void mods_parseargs(int argc, char **argv);
void mods_activate(void);
int mods_count(void);
const char *mods_name(int index);
const char *mods_current(void);
void mods_launch(int index);
FILE *mods_fopen(const char *path, const char *mode);
const char *mods_path(const char *path);
const char *mods_localpath(const char *path);

#endif
