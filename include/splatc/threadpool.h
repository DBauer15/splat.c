/*
 * Adapted from: https://nachtimwald.com/2019/04/12/thread-pool-in-c/
 *
 */
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stddef.h>

struct tpool;
typedef struct tpool tpool;

typedef void (*thread_func_t)(void *arg);

tpool *tpool_create(size_t num);
void tpool_destroy(tpool *tm);

int tpool_add_work(tpool *tm, thread_func_t func, void *arg);
void tpool_wait(tpool *tm);

#endif
