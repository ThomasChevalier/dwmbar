#include "utils.h"

#include <stdlib.h>  // malloc, perror
#include <stdio.h>   // vsnprintf
#include <stdarg.h>  // va_start, va_end
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>

char* smprintf(char *fmt, ...)
{
    va_list fmtargs;
    char *ret;
    int len;

    va_start(fmtargs, fmt);
    len = vsnprintf(NULL, 0, fmt, fmtargs);
    va_end(fmtargs);

    ret = malloc(++len);
    if (ret == NULL) {
        perror("smprintf: malloc");
        exit(1);
    }

    va_start(fmtargs, fmt);
    vsnprintf(ret, len, fmt, fmtargs);
    va_end(fmtargs);

    return ret;
}

char* strip(char *str){
    size_t num_space = 0;
    char *p = str;
    while(*p){
        if(isspace(*p)) ++num_space;
        p++;
    }

    size_t len = strlen(str);
    if(num_space == len-1){
        return smprintf("");
    } else if(num_space == 0){
        return smprintf("%s", str);
    }

    char *stripped = malloc(len+1-num_space);
    p = stripped;
    while(*str){
        if(!isspace(*str)){
            *p = *str;
            ++p;
        }
        ++str;
    }
    *p = 0;
    return stripped;
}

char* read_file(const char *path)
{
    if(!path){
        return NULL;
    }

    FILE *fd = fopen(path, "r");
    if (fd == NULL){
        fprintf(stderr, "fopen: unknown file '%s'", path);
        return NULL;
    }

    if(fseek(fd, 0, SEEK_END) != 0){
        perror("fseek(SEEK_END)");
        return NULL;
    }

    long int fsize = ftell(fd);
    if(fsize == -1){
        perror("ftell");
        return NULL;
    }

    if(fseek(fd, 0, SEEK_SET) != 0){
        perror("fseek(SEEK_SET)");
        return NULL;
    }

    char *content = malloc(fsize + 1);
    if(content == NULL){
        perror("read_file: malloc");
        return NULL;
    }

    size_t ret = fread(content, sizeof(char), fsize, fd);
    /* Ignore cases when ret != fsize because /sys files always get fsize=4096 but a smaller real length */
    if(ret == 0){
        fprintf(stderr, "fread: bad return code (%ld instead of %ld) ", ret, fsize);
        return NULL; 
    }

    content[ret] = 0;
    fclose(fd);

    return content;
}

int is_num(char* str){
    while((isdigit(*str) || isspace(*str)) && *str++);
    return *str == 0;
}

int all_space(char *str){
    while(isspace(*str) && *str++);
    return *str == 0;
}


char* build_block_string(BlockData* data, const char* bar_color)
{
    const unsigned lenData = data->icon ? strlen(data->icon) : 0;
    const unsigned lenText = data->text ? strlen(data->text) : 0;

    if(lenData != 0 && lenText != 0){
        if(all_space(data->text)){
            return smprintf("^c%s^^b%s^ %s ^c%s^^b%s^%s", bar_color, data->color, data->icon, data->color, bar_color, data->text);
        }else{
            return smprintf("^c%s^^b%s^ %s ^c%s^^b%s^ %s ", bar_color, data->color, data->icon, data->color, bar_color, data->text);
        }
    }

    else if (lenData == 0 && lenText != 0){
        if(all_space(data->text)){
            return smprintf("%s", data->text);
        }else{
            return smprintf("^c%s^^b%s^ %s ", data->color, bar_color, data->text);
        }
    }
    else if (lenData != 0 && lenText == 0){
        return smprintf("^c%s^^b%s^ %s ", bar_color, data->color, data->icon);
    }
    else{
        return smprintf("");
    }
}

static char* find_in_dir(char* path, char* hwmon_name, char* file)
{
    DIR           *d;
    struct dirent *dir;

    char* found_path = NULL;
    int good_name = 0;
    int bad_name = 0;

    d = opendir(path);
    if (d){
        while ((dir = readdir(d)) != NULL && (!good_name || !found_path) && !bad_name){
            if(dir->d_type == DT_REG){
                if(!good_name && strcmp(dir->d_name, "name") == 0){
                    char* name_path = smprintf("%s/name", path);
                    char* content = read_file(name_path);
                    bad_name = 1;
                    if(content != NULL){
                        char* stripped = strip(content);
                        if(strcmp(stripped, hwmon_name) == 0){
                            good_name = 1;
                            bad_name = 0;
                        }
                        free(stripped);
                    }
                    free(content);
                    free(name_path);
                }
                else if (strcmp(dir->d_name, file) == 0){
                    found_path = smprintf("%s/%s", path, file);
                }
            }
        }
        closedir(d);
    }

    if(!good_name){
        free(found_path);
        found_path = NULL;
    }
    return found_path;
}

char* find_sensor(char* path, char* hwmon_name, char* file)
{
    DIR           *d;
    struct dirent *dir;
    char* found_path = NULL;

    d = opendir(path);
    if (d){
        while ((dir = readdir(d)) != NULL && !found_path){
            if((dir->d_type == DT_DIR || dir->d_type == DT_LNK)  && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0){
                char* complete_path = smprintf("%s/%s", path, dir->d_name);
                found_path = find_in_dir(complete_path, hwmon_name, file);
                free(complete_path);
            }
        }
        closedir(d);
    }
    return found_path;
}


void setstatus(char *str, Display* dpy)
{
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}