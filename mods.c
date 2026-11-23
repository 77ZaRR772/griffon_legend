#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#ifdef _WIN32
#include <process.h>
#include <direct.h>
#define strcasecmp _stricmp
#define getcwd _getcwd
#define chdir _chdir
#define mkdir(path, mode) _mkdir(path)
#else
#include <strings.h>
#endif

#include "griffon.h"
#include "config.h"
#include "state.h"
#include "mods.h"

#define MAX_MODS 64

static char basedir[256], moddir[256] = "griffonleg", executable[256];
static char names[MAX_MODS][64];
static int count = -1;

static int mod_compare(const void *a, const void *b)
{
	return strcasecmp(a, b);
}

static void mods_scan(void)
{
	count = 0;
	DIR *dir = opendir(basedir);
	if(!dir) return;
	struct dirent *entry;
	while((entry = readdir(dir)) && count < MAX_MODS) {
		if(entry->d_name[0] == '.' || strlen(entry->d_name) >= sizeof(names[0])) continue;
		char path[1024];
		snprintf(path, sizeof(path), "%s/%s/mapdb", basedir, entry->d_name);
		struct stat st;
		if(stat(path, &st) == 0 && S_ISDIR(st.st_mode))
			snprintf(names[count++], sizeof(names[0]), "%s", entry->d_name);
	}
	closedir(dir);
	qsort(names, count, sizeof(names[0]), mod_compare);
}

void mods_parseargs(int argc, char **argv)
{
	if(!getcwd(basedir, sizeof(basedir))) basedir[0] = 0;
	snprintf(executable, sizeof(executable), "%s", argv[0]);
	for(int i = 1; i + 1 < argc; i++) {
		if(strcasecmp(argv[i], "-mod") == 0)
			snprintf(moddir, sizeof(moddir), "%s", argv[i + 1]);
	}
}

void mods_activate(void)
{
	mods_scan();
	for(int i = 0; i < count; i++) {
		if(strcasecmp(names[i], moddir) == 0) {
			memcpy(moddir, names[i], strlen(names[i]) + 1);
			break;
		}
	}

	char path[1024];
	snprintf(path, sizeof(path), "%s/config.ini", basedir);
	if(strlen(path) < 64) strcpy(config_ini, path);
	snprintf(path, sizeof(path), "%s/%s/data/player%%i.sav", basedir, moddir);
	if(strlen(path) < 256) strcpy(player_sav, path);
	snprintf(path, sizeof(path), "%s/%s", basedir, moddir);
	if(chdir(path) != 0) {
		fprintf(stderr, "Cannot load mod '%s', using griffonleg\n", moddir);
		snprintf(moddir, sizeof(moddir), "griffonleg");
		snprintf(path, sizeof(path), "%s/griffonleg", basedir);
		chdir(path);
		snprintf(path, sizeof(path), "%s/griffonleg/data/player%%i.sav", basedir);
		if(strlen(path) < 256) strcpy(player_sav, path);
	}
	mkdir("data", 0777);
	if(!mods_localpath("data/triggers.dat"))
		fprintf(stderr, "Warning: mod '%s' has no data/triggers.dat; copy it here before editing maps\n", moddir);
}

int mods_count(void)
{
	if(count < 0) mods_scan();
	return count;
}

const char *mods_name(int index)
{
	if(index < 0 || index >= mods_count()) return "";
	return names[index];
}

const char *mods_current(void)
{
	return moddir;
}

const char *mods_path(const char *path)
{
	static char full[1024];
	if(path[0] == '/' || (isalpha((unsigned char)path[0]) && path[1] == ':')) return path;
	FILE *fp = fopen(path, "rb");
	if(fp) {
		fclose(fp);
		return path;
	}
	snprintf(full, sizeof(full), "%s/griffonleg/%s", basedir, path);
	return full;
}

const char *mods_localpath(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if(!fp) return NULL;
	fclose(fp);
	return path;
}

FILE *mods_fopen(const char *path, const char *mode)
{
	if(strpbrk(mode, "wa+")) return fopen(path, mode);
	return fopen(mods_path(path), mode);
}

void mods_launch(int index)
{
	if(index < 0 || index >= mods_count()) return;
	extern int saveslot;
	if(player.level > 0) state_save(saveslot);
	config_save(&config);
	chdir(basedir);
	char *args[] = { executable, "-mod", names[index], NULL };
#ifdef _WIN32
	_execv(executable, args);
#else
	execv(executable, args);
#endif
	perror("Cannot restart game");
}
