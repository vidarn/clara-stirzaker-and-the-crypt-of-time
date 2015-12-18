#include "thread.h"
#include <stdlib.h>

typedef struct
{
    unsigned long (*func)(void *);
    void *param;
} ThreadWrapperData;

static void * thread_wrapper(void *data){
    ThreadWrapperData *thread_data = (ThreadWrapperData*)data;
    thread_data->func(thread_data->param);
    free(thread_data);
    return NULL;
}

ThreadHandle thread_start(unsigned long (*func)(void *),void *param)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr,8388608);
    ThreadWrapperData *thread_data =
        (ThreadWrapperData *)malloc(sizeof(ThreadWrapperData));
    thread_data->func = func;
    thread_data->param = param;
    pthread_t handle;
    pthread_create(&handle,&attr,thread_wrapper,(void*)thread_data);
    return handle;
}

void thread_wait(ThreadHandle handle)
{
    pthread_join(handle,NULL);
}

Semaphore semaphore_create(u32 val)
{
    sem_t **sem = malloc(sizeof(sem_t*));
    *sem = malloc(sizeof(sem_t));
    sem_init(*sem,0,val);
    return sem;
}

void semaphore_destroy(Semaphore sem)
{
    sem_destroy(*sem);
    free(*sem);
    free(sem);
}

void semaphore_post(Semaphore sem)
{
    sem_post(*sem);
}

void semaphore_wait(Semaphore sem)
{
    sem_wait(*sem);
}

