#ifndef UTILS_HEADER_TCHEV
#define UTILS_HEADER_TCHEV

#include <X11/Xlib.h>

#include "block.h"

char* smprintf(char *fmt, ...);
char* strip(char*);
char* read_file(char *path);
int is_num(char* str);
int all_space(char *str);

char* build_block_string(BlockData* data, const char* bar_color);

char* find_sensor(char* path, char* hwmon_name, char* file);

void setstatus(char *str, Display* dpy);

#endif // UTILS_HEADER_TCHEV