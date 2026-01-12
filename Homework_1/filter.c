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
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <mqueue.h>
#include "rt-lib.h"

#define SIG_SAMPLE      SIGRTMIN
#define SIG_HZ          1
#define OUTFILE         "signal.txt"

#define F_SAMPLE        50                      
#define PERIOD_US       (1000000L / F_SAMPLE)   

#define SIZEQ           10
#define MSG_SIZE        256
#define Q_STORE         "/print_q"
#define QMSE_STORE      "/mse_q"
#define WD_QUEUE        "/watchdog_queue"
#define QUEUE_PERMISSIONS 0660

#define MSE_SAMPLE      1
#define BUFF_DIM        50
#define PERIOD_US_MSE   (1000000L / MSE_SAMPLE)

#define USAGE_STR                       \
    "Usage: %s [-s] [-n] [-f] [-m | -b | -g]\n" \
    "\t -s: plot original signal\n"     \
    "\t -n: plot noisy signal\n"        \
    "\t -f: plot filtered signal\n"     \
    "\t -m: mean filter\n"              \
    "\t -b: butterworth filter\n"       \
    "\t -g: Savitzky-Golay filter\n"    \
    ""

static int flag_signal   = 0;
static int flag_noise    = 0;
static int flag_filtered = 0;
static int flag_type     = 0; 

/*
    flag_type:

    1 = Mean
    2 = Butterworth
    3 = Savitzky-Golay
*/

static double sig_noise = 0.0;
static double sig_val   = 0.0;
static double glob_time = 0.0;

static pthread_mutex_t mutex_signal;
static pthread_mutex_t mutex_mse_gen;
static pthread_mutex_t mutex_mse_filt;

static double buffer_gen[BUFF_DIM];
static double buffer_filt[BUFF_DIM];

static mqd_t watchdog_queue = (mqd_t)-1;

#define WATCHDOG_PERIOD_SAMPLES 50  


/*
============================= << FILTRI >> =============================
*/



// FILTRO BUTTERWORTH IIR

#define BUTTERFILT_ORD 2
static double b[3] = { 0.0134, 0.0267, 0.0134 };
static double a[3] = { 1.0000, -1.6475, 0.7009 };

static double get_butter(double cur, double *a, double *b)
{
    double retval = 0.0;
    int i;
    static double in[BUTTERFILT_ORD + 1];
    static double out[BUTTERFILT_ORD + 1];

    // Shift dei campioni
    for (i = BUTTERFILT_ORD; i > 0; --i) {
        in[i]  = in[i - 1];
        out[i] = out[i - 1];
    }
    in[0] = cur;

    // Formula ricorsiva IIR
    for (i = 0; i < BUTTERFILT_ORD + 1; ++i) {
        retval += in[i] * b[i];
        if (i > 0)
            retval -= out[i] * a[i];
    }
    out[0] = retval;

    return retval;
}






// FILTRO MA (MEDIA MOBILE) SU DUE CAMPIONI

static int first_mean = 0;

static double get_mean_filter(double cur)
{
    double retval;
    static double vec_mean[2] = {0.0, 0.0};

    vec_mean[1] = vec_mean[0];
    vec_mean[0] = cur;

    if (first_mean == 0) {
        retval = vec_mean[0];
        first_mean = 1;
    } else {
        retval = (vec_mean[0] + vec_mean[1]) / 2.0;
    }
    return retval;
}






// FILTRO SAVITZKY-GOLAY CAUSALE

#define SG_WINDOW_SIZE  5

// Coefficienti causali Savitzky-Golay
static double sg_coeffs_w5_p2[SG_WINDOW_SIZE] = {
    0.88571429, 0.25714286, -0.08571429, -0.14285714, 0.08571429
};

static double get_sg_filter(double cur)
{
    static double history[SG_WINDOW_SIZE] = {0.0};
    static int    startup_count = 0;

    double retval = 0.0;
    int i;

    // Shift storico
    for (i = SG_WINDOW_SIZE - 1; i > 0; --i) {
        history[i] = history[i - 1];
    }
    history[0] = cur;

    // Finché la finestra non è piena, restituisco il campione grezzo
    if (startup_count < SG_WINDOW_SIZE) {
        ++startup_count;
        return cur;
    }

    // Convoluzione con i coefficienti SG
    for (i = 0; i < SG_WINDOW_SIZE; ++i) {
        retval += sg_coeffs_w5_p2[i] * history[i];
    }

    return retval;
}





/*
======================== << GENERATOR THREAD >> ========================
*/

// Genera segnale e noise, aggiornando il buffer per il MSE
static void generator_thread_body(void)
{
    static int sample_index = 0;

    static double local_buffer[BUFF_DIM] = {0.0};

    // Segnale di reference
    double sig_val_local = sin(2 * M_PI * SIG_HZ * glob_time);

    // Rumore somma di coseni
    double sig_noise_local = sig_val_local + 0.5 * cos(2 * M_PI * 10 * glob_time);
    sig_noise_local += 0.9 * cos(2 * M_PI * 4  * glob_time);
    sig_noise_local += 0.9 * cos(2 * M_PI * 12 * glob_time);
    sig_noise_local += 0.8 * cos(2 * M_PI * 15 * glob_time);
    sig_noise_local += 0.7 * cos(2 * M_PI * 18 * glob_time);

    // Aggiornamento variabili condivise
    pthread_mutex_lock(&mutex_signal);
    sig_val   = sig_val_local;
    sig_noise = sig_noise_local;
    glob_time += (1.0 / F_SAMPLE);
    pthread_mutex_unlock(&mutex_signal);

    // Aggiornamento del buffer circolare per MSE
    local_buffer[sample_index % BUFF_DIM] = sig_val_local;

    if ((sample_index % BUFF_DIM) == 0) {
        pthread_mutex_lock(&mutex_mse_gen);
        memcpy(buffer_gen, local_buffer, BUFF_DIM * sizeof(double));
        pthread_mutex_unlock(&mutex_mse_gen);
    }

    ++sample_index;
}

static void *generator_thread(void *arg)
{
    periodic_thread *thd = (periodic_thread *)arg;
    start_periodic_timer(thd, thd->period);
    while (1) {
        wait_next_activation(thd);
        generator_thread_body();
    }
    return NULL;
}





/*
========================= << FILTER THREAD >> ==========================
*/

static void filter_thread_body(mqd_t q_store)
{
    static int sample_index = 0;
    static double local_buffer[BUFF_DIM] = {0.0};
    static unsigned int watchdog_counter = 0;

    double sig_filt;
    double sig_noise_local;
    double sig_val_local;
    double time_local;

    // Copia di valori condivisi
    pthread_mutex_lock(&mutex_signal);
    sig_noise_local = sig_noise;
    sig_val_local   = sig_val;
    time_local      = glob_time;
    pthread_mutex_unlock(&mutex_signal);

    // Selezione filtri
    if (flag_type == 2) {
        sig_filt = get_butter(sig_noise_local, a, b);
    } else if (flag_type == 3) {
        sig_filt = get_sg_filter(sig_noise_local);
    } else {
        sig_filt = get_mean_filter(sig_noise_local);
    }

    // Aggiornamento buffer locale per MSE
    local_buffer[sample_index % BUFF_DIM] = sig_filt;
    if ((sample_index % BUFF_DIM) == 0) {
        pthread_mutex_lock(&mutex_mse_filt);
        memcpy(buffer_filt, local_buffer, BUFF_DIM * sizeof(double));
        pthread_mutex_unlock(&mutex_mse_filt);
    }
    ++sample_index;

    // Costruzione messaggio per lo store
    char msg[MSG_SIZE];
    int n;

    // Token speciale = NaN
    double val_to_send   = flag_signal   ? sig_val_local   : NAN;
    double noise_to_send = flag_noise    ? sig_noise_local : NAN;
    double filt_to_send  = flag_filtered ? sig_filt        : NAN;

    n = snprintf(msg, sizeof(msg), "%lf %lf %lf %lf\n",
                 time_local,
                 val_to_send,
                 noise_to_send,
                 filt_to_send);

    if (n < 0 || n >= (int)sizeof(msg)) {
        fprintf(stderr, "Filter: message truncated or snprintf error\n");
        return;
    }

    if (mq_send(q_store, msg, n + 1, 0) == -1) {
        perror("Filter: mq_send (store)");
        exit(EXIT_FAILURE);
    }

    // HEARTBEAT WATCHDOG 
    ++watchdog_counter;
    // Check superamento 50 campioni e successo nell'apertura della coda
    if ((watchdog_counter % WATCHDOG_PERIOD_SAMPLES) == 0 && watchdog_queue != (mqd_t)-1) {
        const char *hb = "ALIVE";
        if (mq_send(watchdog_queue, hb, strlen(hb) + 1, 0) == -1) {
            perror("Filter: mq_send (watchdog)");
        }
    }
}

static void *filter_thread(void *arg)
{
    periodic_thread *thd = (periodic_thread *)arg;

    mqd_t q_store;

    // Apertura coda store
    if ((q_store = mq_open(Q_STORE, O_WRONLY)) == (mqd_t)-1) {
        perror("Filter: mq_open (store)");
        exit(EXIT_FAILURE);
    }

    // Apertura coda watchdog solo in scrittura (solo 1 messaggio)
    watchdog_queue = mq_open(WD_QUEUE, O_WRONLY);
    if (watchdog_queue == (mqd_t)-1) {
        perror("Filter: mq_open (watchdog)");
        exit(EXIT_FAILURE);
    }

    start_periodic_timer(thd, thd->period);
    while (1) {
        wait_next_activation(thd);
        filter_thread_body(q_store);
    }

    mq_close(q_store);
    mq_close(watchdog_queue);
    mq_unlink(Q_STORE);
    mq_unlink(WD_QUEUE);
    return NULL;
}





/*
=========================== << MSE THREAD >> ===========================
*/

static void mse_calc_thread_body(mqd_t q_mse)
{
    double signal_original[BUFF_DIM] = {0.0};
    double signal_filtered[BUFF_DIM] = {0.0};
    double mse = 0.0;

    pthread_mutex_lock(&mutex_mse_gen);
    memcpy(signal_original, buffer_gen, BUFF_DIM * sizeof(double));
    pthread_mutex_unlock(&mutex_mse_gen);

    pthread_mutex_lock(&mutex_mse_filt);
    memcpy(signal_filtered, buffer_filt, BUFF_DIM * sizeof(double));
    pthread_mutex_unlock(&mutex_mse_filt);

    // Calcolo MSE
    for (int j = 0; j < BUFF_DIM; ++j) {
        double diff = signal_filtered[j] - signal_original[j];
        mse += diff * diff;
    }
    mse /= BUFF_DIM;

    char msg[MSG_SIZE];
    int n = snprintf(msg, sizeof(msg), "%lf\n", mse);
    if (n < 0 || n >= (int)sizeof(msg)) {
        fprintf(stderr, "MSE Calculator: message truncated or snprintf error\n");
        return;
    }

    if (mq_send(q_mse, msg, n + 1, 0) == -1) {
        perror("MSE Calculator: mq_send (mse_q)");
    }
}

static void *mse_calc_thread(void *arg)
{
    periodic_thread *thd = (periodic_thread *)arg;
    mqd_t mse_store;

    if ((mse_store = mq_open(QMSE_STORE, O_WRONLY | O_NONBLOCK, QUEUE_PERMISSIONS, NULL)) == (mqd_t)-1) {
        perror("MSE Calculator: mq_open (mse_store)");
        exit(EXIT_FAILURE);
    }

    start_periodic_timer(thd, thd->period);
    while (1) {
        wait_next_activation(thd);
        mse_calc_thread_body(mse_store);
    }

    mq_close(mse_store);
    return NULL;
}





/*
============================ << PARSING >> =============================
*/

static void parse_cmdline(int argc, char **argv)
{
    int opt;

    while ((opt = getopt(argc, argv, "snfmbg")) != -1) {
        switch (opt) {
        case 's':
            flag_signal = 1;
            break;
        case 'n':
            flag_noise = 1;
            break;
        case 'f':
            flag_filtered = 1;
            break;
        case 'm':
            flag_type = 1; // mean filter
            break;
        case 'b':
            flag_type = 2; // butterworth filter
            break;
        case 'g':
            flag_type = 3; // Savitzky-Golay filter
            break;
        default:
            fprintf(stderr, USAGE_STR, argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // Se non c'è nessun flag si attiva tutto
    if ((flag_signal | flag_noise | flag_filtered | flag_type) == 0) {
        flag_signal   = 1;
        flag_noise    = 1;
        flag_filtered = 1;
        flag_type     = 1;
    }
}





/*
============================== << MAIN >> ==============================
*/

int main(int argc, char **argv)
{
    periodic_thread *generator      = malloc(sizeof(periodic_thread));
    periodic_thread *filter_thd     = malloc(sizeof(periodic_thread));
    periodic_thread *mse_calculator = malloc(sizeof(periodic_thread));

    pthread_t gen_tid;
    pthread_t filt_tid;
    pthread_t mse_tid;

    pthread_attr_t gen_attr;
    pthread_attr_t filt_attr;
    pthread_attr_t mse_attr;

    struct sched_param param;
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_t mse_mutex_attr_gen;
    pthread_mutexattr_t mse_mutex_attr_filt;

    parse_cmdline(argc, argv);

    pthread_attr_init(&gen_attr);
    pthread_attr_init(&filt_attr);
    pthread_attr_init(&mse_attr);

    pthread_attr_setschedpolicy(&gen_attr, SCHED_FIFO);
    pthread_attr_setschedpolicy(&filt_attr, SCHED_FIFO);
    pthread_attr_setschedpolicy(&mse_attr, SCHED_FIFO);

    param.sched_priority = 70;
    pthread_attr_setschedparam(&gen_attr, &param);

    param.sched_priority = 69;
    pthread_attr_setschedparam(&filt_attr, &param);

    param.sched_priority = 60;
    pthread_attr_setschedparam(&mse_attr, &param);

    pthread_attr_setinheritsched(&gen_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&filt_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&mse_attr, PTHREAD_EXPLICIT_SCHED);

    pthread_attr_setdetachstate(&gen_attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setdetachstate(&filt_attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setdetachstate(&mse_attr, PTHREAD_CREATE_JOINABLE);

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setprotocol(&mutex_attr, PTHREAD_PRIO_PROTECT);
    pthread_mutexattr_setprioceiling(&mutex_attr, 70); // Ceil tra filter e generator
    pthread_mutex_init(&mutex_signal, &mutex_attr);

    pthread_mutexattr_init(&mse_mutex_attr_gen);
    pthread_mutexattr_setprotocol(&mse_mutex_attr_gen, PTHREAD_PRIO_PROTECT);
    pthread_mutexattr_setprioceiling(&mse_mutex_attr_gen, 70); // Ceil tra generator e MSE
    pthread_mutex_init(&mutex_mse_gen, &mse_mutex_attr_gen);

    pthread_mutexattr_init(&mse_mutex_attr_filt);
    pthread_mutexattr_setprotocol(&mse_mutex_attr_filt, PTHREAD_PRIO_PROTECT);
    pthread_mutexattr_setprioceiling(&mse_mutex_attr_filt, 69); // Ceil tra filter e MSE
    pthread_mutex_init(&mutex_mse_filt, &mse_mutex_attr_filt);


    generator->index   = 0;
    generator->period  = PERIOD_US;
    generator->priority= 70;

    filter_thd->index  = 1;
    filter_thd->period = PERIOD_US;
    filter_thd->priority = 69;

    mse_calculator->index   = 2;
    mse_calculator->period  = PERIOD_US_MSE;
    mse_calculator->priority= 60;

    // Creazione thread
    pthread_create(&gen_tid,  &gen_attr,  generator_thread,  (void *)generator);
    pthread_create(&filt_tid, &filt_attr, filter_thread,     (void *)filter_thd);
    pthread_create(&mse_tid,  &mse_attr,  mse_calc_thread,   (void *)mse_calculator);

    pthread_join(gen_tid,  NULL);
    pthread_join(filt_tid, NULL);
    pthread_join(mse_tid,  NULL);

    // Cleanup dei thread
    pthread_attr_destroy(&gen_attr);
    pthread_attr_destroy(&filt_attr);
    pthread_attr_destroy(&mse_attr);
    pthread_mutexattr_destroy(&mutex_attr);
    pthread_mutexattr_destroy(&mse_mutex_attr_gen);
    pthread_mutexattr_destroy(&mse_mutex_attr_filt);
    pthread_mutex_destroy(&mutex_signal);
    pthread_mutex_destroy(&mutex_mse_gen);
    pthread_mutex_destroy(&mutex_mse_filt);

    free(generator);
    free(filter_thd);
    free(mse_calculator);

    return 0;
}