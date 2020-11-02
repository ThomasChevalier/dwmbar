#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <time.h>

#include <sys/inotify.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <poll.h>

#include <X11/Xlib.h>

#include "debug.h"
#include "block.h"
#include "utils.h"
#include "listeners.h"


/* defines */

#define LENGTH(X) (sizeof X / sizeof X[0])

/* function declaration */

void time_callback         (Block* blk);
void volume_callback       (Block* blk);
void battery_callback      (Block* blk);
void power_callback        (Block* blk);
void temperature_callback  (Block* blk);
void fan_callback          (Block* blk);
void mem_callback          (Block* blk);
void brightness_callback   (Block* blk);

void *listener_time        (void*);
void *listener_volume      (void*);
void *listener_battery     (void*);
void *listener_power       (void*);
void *listener_temperature (void*);
void *listener_fan         (void*);
void *listener_mem         (void*);
void *listener_brightness  (void*);

void detect_sensors(void);


/* global variables */

static Display *dpy;

static Block blocks[] = {
    BLOCK_DEF(listener_temperature),
    BLOCK_DEF(listener_fan),
    BLOCK_DEF(listener_mem),
    BLOCK_DEF(listener_battery),
    BLOCK_DEF(listener_power),
    BLOCK_DEF(listener_brightness),
    BLOCK_DEF(listener_volume),
    BLOCK_DEF(listener_time),
};

static pthread_mutex_t mutex_cond = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t update_cond = PTHREAD_COND_INITIALIZER;


static const char* bar_color = "#282828";
static char* fail_icon_s = " ";
static char* fail_icon = "";

static char* fan1_sensor;        // "/sys/class/hwmon/hwmon5/fan1_input"
static char* fan2_sensor;        // "/sys/class/hwmon/hwmon5/fan2_input"
static char* cpu_sensor;         // "/sys/class/hwmon/hwmon6/temp1_input"
static char* bat_status_sensor;  // "/sys/class/power_supply/BAT0/status"
static char* bat_curr_sensor;    // "/sys/class/power_supply/BAT0/current_now"
static char* bat_volt_sensor;    // "/sys/class/power_supply/BAT0/voltage_now"
static char* bat_present_sensor; // "/sys/class/power_supply/BAT0/present"
static char* bat_capa_sensor;    // "/sys/class/power_supply/BAT0/capacity"
static char* mem_sensor;         // "/proc/meminfo"

static const char* brightness_file = "/mnt/data/Programmation/Archlinux/Scripts/brightness_control/current";
static const char* volume_file = "/mnt/data/Programmation/Archlinux/Scripts/volume_control/current";

/* function implementations */

void time_callback(Block* blk)
{
    blk->data.color = "#ffffff";
    free(blk->data.text);

    unsigned int hour = -1;

    const time_t now = time(NULL);
    const struct tm *timtm = localtime(&now);
    if (timtm == NULL){
        blk->data.text = smprintf(fail_icon_s);
    } else{
        char buf[129];
        if (!strftime(buf, sizeof(buf)-1, "%H:%M", timtm)) {
            fprintf(stderr, "strftime == 0\n");
            blk->data.text = smprintf(fail_icon_s);
        }else{
            blk->data.text = smprintf("%s", buf);
            hour = timtm->tm_hour;
        }
    }

    if(hour == -1){
        blk->data.icon = "";
    }
    else{
        char *clocks[12] = {"", "", "", "", "", "", "", "", "", "", "", ""};
        if (hour >= 12){
            hour -= 12;
        }
        if (hour < 0){
            blk->data.icon = fail_icon_s;
        }else{
            blk->data.icon = clocks[hour];
        }
    }
}

void volume_callback(Block* blk)
{
    blk->data.color = "#ebcb8b";
    free(blk->data.text);

    /* Read current volume */
    debug_printf("reading %s\n", volume_file);
    char* content = read_file(volume_file);
    if(content == NULL){
        fprintf(stderr, "Cannot read %s\n", volume_file);
        return;
    }

    

    // Block data generation
    if(is_num(content)){
        int vol = atoi(content);
        free(content);

        blk->data.text = smprintf("%d%%", vol);
        if(vol == 0){
            blk->data.icon = "ﱝ";
        }else if(vol < 25){
            blk->data.icon = "奄";
        }else if(vol < 50){
            blk->data.icon = "奔";
        }else{
            blk->data.icon = "墳";
        }

    }else{
        blk->data.text = strip(content);
        free(content);
        blk->data.icon = "ﱝ";
    }
}

void battery_callback(Block* blk)
{
    blk->data.color = "#a3be8c";
    free(blk->data.text);

    int cap = -1;

    char* bat_present = read_file(bat_present_sensor);
    if (bat_present == NULL){
        blk->data.text = smprintf(fail_icon_s);
    }
    else if (bat_present[0] != '1'){
        blk->data.text = smprintf("");
    }
    else{
        char* bat_capa = read_file(bat_capa_sensor);
        if (bat_capa == NULL) {
            blk->data.text = smprintf(fail_icon_s);
        }else{
            cap = atoi(bat_capa);
            blk->data.text = smprintf("%d%%", cap);
        }
        free(bat_capa);
    }
    free(bat_present);

    if(cap == -1 || cap >= 80){
        blk->data.icon = " ";
    }else if(cap >= 60){
        blk->data.icon = " ";
    }else if(cap >= 40){
        blk->data.icon = " ";
    }else if(cap >= 20){
        blk->data.icon = " ";
    }else{
        blk->data.icon = " ";
    }
}

void power_callback(Block* blk)
{
    blk->data.icon = "";
    blk->data.color = "#d06c4c";
    free(blk->data.text);

    /* circular buffer */
    static float history[5];
    static size_t end = 0;
    static size_t len = 0;

    long int current = 0;
    long int voltage = 0;

    /* Hide the block if battery full */
    char* bat_status = read_file(bat_status_sensor);
    if(bat_status == NULL){
        blk->data.text = smprintf(fail_icon);
        return;
    }else{
        char* stripped = strip(bat_status);

        // Hide the indicator if battery is full
        if(!strcmp(stripped, "Full")){
            blk->data.icon = "";
            blk->data.text = smprintf("");
            free(stripped);
            free(bat_status);
            return;
        }

        free(stripped);
        free(bat_status);
    }
    

    char* bat_curr = read_file(bat_curr_sensor);
    if (bat_curr == NULL){
        blk->data.text = smprintf(fail_icon);
        return;
    }else{
        current = strtol(bat_curr, NULL, 10);
        free(bat_curr);
    }

    char* bat_volt = read_file(bat_volt_sensor);
    if (bat_volt == NULL){
        blk->data.text = smprintf(fail_icon);
        return;
    }else{
        voltage = strtol(bat_volt, NULL, 10);
        free(bat_volt);
    }

    if(voltage == 0 || current == 0){
        blk->data.text = smprintf(fail_icon);
        return;
    }
    else{
        float power = current/1e6*voltage/1e6;
        history[end] = power;
        if(len < LENGTH(history)){
            len += 1;
        }
        end = (end+1)%LENGTH(history);

        /* Averaging */
        float sum = 0;
        for(size_t i=0; i < len; ++i){
            sum += history[i];
        }
        sum /= len;

        blk->data.text = smprintf("%.1fW", sum);
    }
}


void temperature_callback(Block* blk)
{
    blk->data.color = "#e85c6a";
    free(blk->data.text);

    double temp = 0;
    
    char* cpu_temp = read_file(cpu_sensor);
    if (cpu_temp == NULL){
        blk->data.text = smprintf(fail_icon);
    } else{
        temp = atof(cpu_temp)/1000;
        blk->data.text = smprintf("%02.0f°C", temp);
    }
    free(cpu_temp);


    if (temp >= 60){
        blk->data.icon = "";
    } else if (temp >= 40){
        blk->data.icon = "";
    } else{
        blk->data.icon = "";
    }
}

void fan_callback(Block* blk)
{
    blk->data.color = "#88c0d0";
    free(blk->data.text);

    char* rpm1;
    char* rpm2;
    int rpm1_i = -1;
    int rpm2_i = -1;

    char* fan1 = read_file(fan1_sensor);
    if (fan1 == NULL){
        rpm1 = smprintf(fail_icon_s);
    }else{
        rpm1_i = atoi(fan1);
        free(fan1);
        rpm1 = smprintf("%d", rpm1_i);
    }

    char* fan2 = read_file(fan2_sensor);
    if (fan2 == NULL){
        rpm2 = smprintf(fail_icon_s);
    }else{
        rpm2_i = atoi(fan2);
        free(fan2);
        rpm2 = smprintf("%d", rpm2_i);
    }

    if(rpm1_i == -1 && rpm2_i == -1){
        blk->data.text = smprintf("%s %s", rpm1, rpm2);
    }else{
        if(rpm1_i == 0 && rpm2_i == 0){
            blk->data.text = smprintf(" ");
        }else{
            blk->data.text = smprintf("%s %s rpm", rpm1, rpm2);
        }
    }

    free(rpm1);
    free(rpm2);

    if((rpm1_i != -1 || rpm2_i != -1) && (rpm1_i == 0 && rpm2_i == 0)){
        blk->data.icon = "ﴛ";
    }else{
        blk->data.icon = "";
    }

}

void mem_callback(Block* blk)
{
    blk->data.icon = "";
    blk->data.color = "#ebcb8b";
    free(blk->data.text);

    // Manually opening file here because it doesn't pass the checks of the read_file function,
    // and it is easier for parsing lines
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if(meminfo == NULL){
        debug_printf("[mem_callback]: cannot open /proc/meminfo file\n");
        blk->data.text = smprintf(fail_icon_s);
        return;
    }

    char line[256];
    int ram_available = -1;
    int ram_total = -1;
    while(fgets(line, sizeof(line), meminfo) && (ram_available == -1 || ram_total == -1))
    {
        if(ram_total == -1){
            sscanf(line, "MemTotal: %d kB", &ram_total);   
        }

        if(ram_available == -1){
            sscanf(line, "MemAvailable: %d kB", &ram_available);
        }
        
    }
    fclose(meminfo);

    if(ram_available < 0 || ram_total < 0){
        debug_printf("[mem_callback]: no MemTotal or MemAvailable entry in /proc/meminfo\n");
        blk->data.text = smprintf(fail_icon_s);
        return;
    }

    unsigned int ram_used = (ram_total - ram_available) / 1024; // kB -> MB

    if(ram_used > 1024){
        double used_f = ram_used / 1024.;
        blk->data.text = smprintf("%.1fG", used_f);
    }else{
        blk->data.text = smprintf("%ldM", ram_used);
    }
    
}

void brightness_callback(Block* blk)
{
    blk->data.color = "#88c0d0";
    blk->data.icon = "☀";
    free(blk->data.text);

    char* brightness = read_file(brightness_file);
    if(brightness == NULL){
        fprintf(stderr, "Cannot read %s\n", brightness_file);
        return;
    }

    if(is_num(brightness)){
        float bright = atoi(brightness);
        int percentage = round(bright/1200);
        blk->data.text = smprintf("%d%%", percentage);
    }else{
        blk->data.text = strip(brightness);
    }
    free(brightness);
}

void* listener_time(void* p_data)
{   
    Block* blk = (Block*)p_data;
    safe_callback(blk, time_callback, &update_cond);
    aligned_time_listener(1592384460, 60, blk, time_callback, &update_cond);
    return (void*)0;
}

void *listener_volume(void* p_data)
{
    Block* blk = (Block*)p_data;
    safe_callback(blk, volume_callback, &update_cond);
    file_listener(blk, volume_file, volume_callback, &update_cond);
    return (void*)0;
}

void *listener_battery(void* p_data)
{
    Block* blk = (Block*)p_data;
    safe_callback(blk, battery_callback, &update_cond);
    time_listener(60, blk, battery_callback, &update_cond);
    return (void*)0;
}

void *listener_power(void* p_data)
{
    Block* blk = (Block*)p_data;
    safe_callback(blk, power_callback, &update_cond);
    time_listener(20, blk, power_callback, &update_cond);
    return (void*)0;
}

void *listener_temperature(void* p_data)
{   
    Block* blk = (Block*)p_data;
    safe_callback(blk, temperature_callback, &update_cond);
    time_listener(20, blk, temperature_callback, &update_cond);
    return (void*)0;
}

void *listener_fan(void* p_data)
{   
    Block* blk = (Block*)p_data;
    safe_callback(blk, fan_callback, &update_cond);
    time_listener(5, blk, fan_callback, &update_cond);
    return (void*)0;
}

void *listener_mem(void* p_data)
{   
    Block* blk = (Block*)p_data;
    safe_callback(blk, mem_callback, &update_cond);
    time_listener(10, blk, mem_callback, &update_cond);
    return (void*)0;
}

void *listener_brightness(void* p_data)
{   
    Block* blk = (Block*)p_data;
    safe_callback(blk, brightness_callback, &update_cond);
    file_listener(blk, brightness_file, brightness_callback, &update_cond);
    return (void*)0;
}


void detect_sensors(void)
{
    fan1_sensor         = find_sensor("/sys/class/hwmon", "dell_smm", "fan1_input");
    fan2_sensor         = find_sensor("/sys/class/hwmon", "dell_smm", "fan2_input");
    cpu_sensor          = find_sensor("/sys/class/hwmon", "coretemp", "temp1_input");

    bat_status_sensor   = "/sys/class/power_supply/BAT0/status";
    bat_curr_sensor     = "/sys/class/power_supply/BAT0/current_now";
    bat_volt_sensor     = "/sys/class/power_supply/BAT0/voltage_now";
    bat_present_sensor  = "/sys/class/power_supply/BAT0/present";
    bat_capa_sensor     = "/sys/class/power_supply/BAT0/capacity";
    mem_sensor          = "/proc/meminfo";
}

int main(void)
{
    // Initialize display
    if (!(dpy = XOpenDisplay(NULL))) {
        fprintf(stderr, "dwmstatus: cannot open display.\n");
        return 1;
    }

    pthread_t threads[LENGTH(blocks)];

    debug_printf("detecting sensors\n");
    detect_sensors();
    debug_printf("fan1_sensor: %s\n", fan1_sensor);
    debug_printf("fan2_sensor: %s\n", fan2_sensor);
    debug_printf("cpu_sensor: %s\n", cpu_sensor);
    debug_printf("bat_status_sensor: %s\n", bat_status_sensor);
    debug_printf("bat_curr_sensor: %s\n", bat_curr_sensor);
    debug_printf("bat_volt_sensor: %s\n", bat_volt_sensor);
    debug_printf("bat_present_sensor: %s\n", bat_present_sensor);
    debug_printf("bat_capa_sensor: %s\n\n", bat_capa_sensor);

    // Launch blocks
    debug_printf("creating %ld threads\n", LENGTH(blocks));
    for(int i=0; i < LENGTH(blocks); ++i){
        pthread_create(&threads[i], NULL, blocks[i].listener, &blocks[i]);
    }

    // Update status
    while(1){

        // wait for update
        pthread_mutex_lock(&mutex_cond);
        pthread_cond_wait (&update_cond, &mutex_cond);

        size_t len_status = 0;

        // update block string
        for(int i=0; i < LENGTH(blocks); ++i){
            pthread_mutex_lock(&blocks[i].mutex_data);
            if(blocks[i].new){
                debug_printf("block %d has new data: [%s] %s: %s\n", i, blocks[i].data.color, blocks[i].data.icon, blocks[i].data.text);
                blocks[i].new = 0;

                free(blocks[i].string);
                blocks[i].string = build_block_string(&blocks[i].data, bar_color);
                debug_printf("block %d: %s\n", i, blocks[i].string);
            }
            pthread_mutex_unlock(&blocks[i].mutex_data);

            // update status length
            if(blocks[i].string != NULL){
                len_status += strlen(blocks[i].string);
            }
        }

        char status[len_status+1]; 
        memset(status, 0, len_status+1);
        for(int i=0; i < LENGTH(blocks); ++i){
            if(blocks[i].string != NULL){
                strcat(status, blocks[i].string);
            }
        }
        setstatus(status, dpy);
        debug_printf("status=%s\n", status);

        pthread_mutex_unlock(&mutex_cond);
    }

    XCloseDisplay(dpy);

}
