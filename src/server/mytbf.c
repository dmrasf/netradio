#include "mytbf.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

struct mytbf_st {
    int cps;
    int burst;
    int token;
    int pos;
    pthread_mutex_t mut;
    pthread_cond_t cond;
};

static struct sigaction sa_before;
static struct itimerval iti_old;
static struct itimerval iti_new;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static struct mytbf_st* job[MYTBF_MAX];
static pthread_mutex_t mut_job = PTHREAD_MUTEX_INITIALIZER;
static pthread_t tid;

static int find_free_pos_unlocked(void)
{
    for (int i = 0; i < MYTBF_MAX; i++)
        if (job[i] == NULL)
            return i;
    return -1;
}

static void* alrm_handler(void* ptr)
{
    while (1) {
        pthread_mutex_lock(&mut_job);
        for (int i = 0; i < MYTBF_MAX; i++) {
            if (job[i] != NULL) {
                pthread_mutex_lock(&job[i]->mut);
                job[i]->token += job[i]->cps;
                if (job[i]->token >= job[i]->burst)
                    job[i]->token = job[i]->burst;
                pthread_mutex_unlock(&job[i]->mut);
                pthread_cond_broadcast(&job[i]->cond);
            }
        }
        pthread_mutex_unlock(&mut_job);
        sleep(1);
    }
}

static void module_unload(void)
{
    pthread_cancel(tid);
    pthread_join(tid, NULL);

    for (int i = 0; i < MYTBF_MAX; i++) {
        if (job[i] != NULL) {
            pthread_mutex_destroy(&job[i]->mut);
            free(job[i]);
        }
    }
    pthread_mutex_destroy(&mut_job);
}

static void module_load(void)
{
    int err;
    if ((err = pthread_create(&tid, NULL, alrm_handler, NULL))) {
        fprintf(stderr, "%s\n", strerror(err));
        exit(1);
    }

    atexit(module_unload);
}

mytbf_t* mytbf_init(int cps, int burst)
{
    struct mytbf_st* mt;
    mt = malloc(sizeof(*mt));
    if (mt == NULL)
        return NULL;

    pthread_mutex_lock(&mut_job);
    int pos = find_free_pos_unlocked();
    if (pos < 0)
        return NULL;

    mt->cps = cps;
    mt->burst = burst;
    mt->token = 0;
    mt->pos = pos;
    pthread_mutex_init(&mt->mut, NULL);
    pthread_cond_init(&mt->cond, NULL);

    job[pos] = mt;
    pthread_mutex_unlock(&mut_job);

    pthread_once(&init_once, module_load);

    return mt;
}

int mytbf_fetchtoken(mytbf_t* ptr, int size)
{
    if (size <= 0)
        return -EINVAL;

    struct mytbf_st* mt = ptr;

    pthread_mutex_lock(&mt->mut);
    while (mt->token <= 0)
        // mut 解锁 -> cond 等待 -> mut 上锁
        pthread_cond_wait(&mt->cond, &mt->mut);
    int n = mt->token < size ? mt->token : size;
    mt->token -= n;
    pthread_mutex_unlock(&mt->mut);

    return n;
}

int mytbf_returntoken(mytbf_t* ptr, int size)
{
    if (size <= 0)
        return -EINVAL;

    struct mytbf_st* mt = ptr;

    pthread_mutex_lock(&mt->mut);
    mt->token += size;
    if (mt->token > mt->burst)
        mt->token = mt->burst;
    pthread_mutex_unlock(&mt->mut);
    pthread_cond_broadcast(&mt->cond);

    return 0;
}

void mytbf_destroy(mytbf_t* ptr)
{
    struct mytbf_st* mt = ptr;
    pthread_mutex_lock(&mut_job);
    job[mt->pos] = NULL;
    pthread_mutex_unlock(&mut_job);
    pthread_mutex_destroy(&mt->mut);
    pthread_cond_destroy(&mt->cond);
    free(mt);
}
