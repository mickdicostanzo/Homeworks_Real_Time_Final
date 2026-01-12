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
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdint.h>
#include "rt-lib.h"

#define SIG_SAMPLE     SIGRTMIN
#define SIG_HZ         1
#define OUTFILE        "signal.txt"
#define F_SAMPLE       5
#define SIZEQ          10
#define MSG_SIZE       256
#define Q_STORE        "/print_q"
#define QMSE_STORE     "/mse_q"
#define QUEUE_PERMISSIONS 0660
#define NSEC_PER_SEC   1000000000ULL


// Legge dalla coda, scrive sul file e stampa MSE
static void store_body(mqd_t q_store, mqd_t mse_store, FILE *outfd)
{
    char msg[MSG_SIZE];
    const char delim[] = " ";
    char *token;

    for (int i = 0; i < SIZEQ; ++i) {
        if (mq_receive(q_store, msg, MSG_SIZE, NULL) == -1) {
            perror("Store: mq_receive (print_q)");
            exit(EXIT_FAILURE);
        }

        msg[strcspn(msg, "\n")] = '\0';
        int count = 0;

        token = strtok(msg, delim);
        while (token != NULL) {
            ++count;

            // Alla lettura del token speciale NaN, si omette la scrittura 
            // del valore corrispondente nel file signal.txt
            if (strcasecmp(token, "nan") != 0) {
                fprintf(outfd, "%s", token);
            }

            if (count % 4 == 0) {
                fprintf(outfd, "\n");  // nuova riga ogni 4 valori (t, s, n, f)
            } else {
                fprintf(outfd, ",");
            }

            token = strtok(NULL, delim);
        }
    }

    fflush(outfd);

    char mse_msg[MSG_SIZE];
    if (mq_receive(mse_store, mse_msg, MSG_SIZE, NULL) != -1) {
        printf("current_mse: %s", mse_msg);
        fflush(stdout);
    }
}





/*
============================== << MAIN >> ==============================
*/

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int outfile;
    FILE *outfd;

    mqd_t q_store;
    mqd_t mse_store;
    struct mq_attr attr;
    struct mq_attr mse_attr;

    // Utilizziamo una struttura periodic thread per usare le funzioni di rt-lib.h
    periodic_thread store_thread;

    memset(&attr, 0, sizeof(attr));
    attr.mq_flags   = 0;
    attr.mq_maxmsg  = SIZEQ;
    attr.mq_msgsize = MSG_SIZE;

    memset(&mse_attr, 0, sizeof(mse_attr));
    mse_attr.mq_flags   = O_NONBLOCK;
    mse_attr.mq_maxmsg  = 1;
    mse_attr.mq_msgsize = MSG_SIZE;

    if ((q_store = mq_open(Q_STORE, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) == (mqd_t)-1) {
        perror("Store: mq_open (store)");
        exit(EXIT_FAILURE);
    }

    if ((mse_store = mq_open(QMSE_STORE,
                             O_RDONLY | O_CREAT | O_NONBLOCK,
                             QUEUE_PERMISSIONS,
                             &mse_attr)) == (mqd_t)-1) {
        perror("Store: mq_open (mse_store)");
        exit(EXIT_FAILURE);
    }

    outfile = open(OUTFILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    outfd = fdopen(outfile, "w");
    if (outfile < 0 || !outfd) {
        perror("Store: unable to open/create output file");
        return EXIT_FAILURE;
    }

    store_thread.period   = (int)(1000000.0 / F_SAMPLE); // periodo in us

    start_periodic_timer(&store_thread, 0);

    while (1) {
        wait_next_activation(&store_thread);
        store_body(q_store, mse_store, outfd);
    }

    mq_close(q_store);
    mq_close(mse_store);
    mq_unlink(Q_STORE);
    mq_unlink(QMSE_STORE);
    fclose(outfd);
    
    return 0;
}
