#pragma once
#include "../../common.h"
#ifdef _WIN32
#include "thread_win32.h"
#else
#include "thread_pthread.h"
#endif

ThreadHandle thread_start(unsigned long (*func)(void *),void *param);
void thread_wait(ThreadHandle handle);
Semaphore semaphore_create(u32 val);
void semaphore_destroy(Semaphore sem);
void semaphore_post(Semaphore sem);
void semaphore_wait(Semaphore sem);
