#include "listeners.h"

#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/inotify.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>


void file_listener(Block* blk, char* file, void (*callback)(Block*), pthread_cond_t* update_cond)
{
    // See man inotify(1) for reference

    int fd = inotify_init();
    if(fd == -1){
        perror("inotify_init");
        return;
    }

    //int wd = inotify_add_watch(fd,"/home/thomas/volume_control/current", IN_CLOSE_WRITE);
    int wd = inotify_add_watch(fd, file, IN_CLOSE_WRITE);
    if(wd == -1){
        perror("inotify_add_watch1");
        return;
    }

    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    while(1){
        int poll_num = poll(&pfd, 1, -1);
        if (poll_num == -1) {
            if (errno == EINTR)
                continue;
            perror("poll");
            return;
        }

        if (poll_num > 0 && pfd.revents & POLLIN) {
            // It's necessary to parse events to update the volume only on modifications
            char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
            const struct inotify_event *event;
            ssize_t len;
            char *ptr;

            /* Loop while events can be read from inotify file descriptor. */
            for (;;) {

                /* Read some events. */
                len = read(fd, buf, sizeof buf);
                if (len == -1 && errno != EAGAIN) {
                    perror("read");
                    exit(EXIT_FAILURE);
                }

                /* If the nonblocking read() found no events to read, then
                it returns -1 with errno set to EAGAIN. In that case,
                we exit the loop. */
                if (len <= 0)
                    break;

                /* Loop over all events in the buffer */

                for (ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {

                    event = (const struct inotify_event *) ptr;

                    if(event->mask & IN_CLOSE_WRITE){
                        safe_callback(blk, callback, update_cond);
                    }

                }
            }
        }

    }
}


void aligned_time_listener(time_t align, time_t interval, Block* blk, void (*callback)(Block*), pthread_cond_t* update_cond)
{   

    // Align the sleep interval on a multiple of [interval] seconds
    time_t now = time(NULL);
    const time_t delta = now - align;
    unsigned int nextSleep = now + interval - delta % interval;

    while(1){

        // waiting ...
        // sleep while we need it
        unsigned int left = sleep(nextSleep-time(NULL));
        while(left){
            if(nextSleep > time(NULL)){
                left = sleep(nextSleep-time(NULL));
            }
        }

        nextSleep += interval;

        // Realign the next sleep in case of hibernation
        now = time(NULL);
        if(nextSleep <= now){
            nextSleep = now + interval - (now - nextSleep) % interval;
        }

        safe_callback(blk, callback, update_cond);
    }
}

void time_listener(time_t interval, Block* blk, void (*callback)(Block*), pthread_cond_t* update_cond)
{   
    while(1){
        sleep(interval);
        
        safe_callback(blk, callback, update_cond);
    }
}

void safe_callback(Block* blk, void (*callback)(Block*), pthread_cond_t* update_cond)
{
    pthread_mutex_lock(&blk->mutex_data);
    blk->new = 1;
    callback(blk);
    pthread_mutex_unlock(&blk->mutex_data);
    pthread_cond_signal(update_cond);
}