/*
    Real-Time Programming Homework 1
    Signal Generation, Filtering and Storage using POSIX real-time extensions

    MEMBRI DEL GRUPPO:
    - Annunziata Giovanni              DE6000015
    - Di Costanzo Michele Pio          DE6000001
    - Di Palma Lorenzo                 N39001908 
    - Zaccone Amedeo                   DE6000014  
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <mqueue.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include "rt-lib.h"

#define WD_QUEUE          "/watchdog_queue"
#define QUEUE_PERMISSIONS 0660
#define MSG_SIZE          256
#define U_SEC             1000000L
#define TIMEOUT_US        (3 * U_SEC)

static void watch_dog(mqd_t queue)
{
    char msg[MSG_SIZE];
    struct timespec abs_timeout;


    clock_gettime(CLOCK_REALTIME, &abs_timeout);
    timespec_add_us(&abs_timeout, TIMEOUT_US);

    if (mq_timedreceive(queue, msg, MSG_SIZE, NULL, &abs_timeout) == -1) {
        perror("Watchdog: mq_timedreceive");
        printf("Watchdog: filtro NON attivo (nessun heartbeat entro 3s)\n");
    } else {
        printf("Watchdog: filtro attivo, messaggio = %s\n", msg);
    }
    fflush(stdout);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    mqd_t wd_queue;
    struct mq_attr wd_attr = {0};
    
    // Utilizziamo una struttura periodic thread per usare le funzioni di rt-lib.h
    periodic_thread wd_thread;
    wd_attr.mq_maxmsg  = 1;        // un solo heartbeat alla volta
    wd_attr.mq_msgsize = MSG_SIZE; 

    wd_queue = mq_open(WD_QUEUE, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &wd_attr);
    if (wd_queue == (mqd_t)-1) {
        perror("Watchdog: mq_open (watchdog_queue)");
        exit(EXIT_FAILURE);
    }

    printf("Watchdog: avviato, in attesa di heartbeat dal filtro...\n");
    fflush(stdout);

    wd_thread.period   = (int)(U_SEC); // periodo in us

    start_periodic_timer(&wd_thread, 0);
    while (1) {
        wait_next_activation(&wd_thread);
        watch_dog(wd_queue);
    }

    mq_close(wd_queue);
    mq_unlink(WD_QUEUE);

    return 0;
}
