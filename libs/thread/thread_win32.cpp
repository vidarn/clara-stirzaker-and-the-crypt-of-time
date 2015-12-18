#include "thread.h"

HANDLE thread_start(unsigned long (*func)(void *),void *param)
{
    HANDLE handle = CreateThread(NULL,0,func,param,0,NULL);
    return handle;
}

void thread_wait(HANDLE handle)
{
    WaitForSingleObject(handle,INFINITE);
}
