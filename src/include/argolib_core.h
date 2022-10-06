#ifndef __ARGOLIB_CORE_H__
#define __ARGOLIB_CORE_H__
 
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <abt.h>

// For giving an ID to each thread
typedef struct
{
    void* args; //Pointer to the real arguments to the function
    int tid;    //ID to each thread
} thread_arg_t;

typedef ABT_thread Task_handle;
typedef void (*fork_t)(void* args);

void argolib_core_init(int argc, char ** argv);
void argolib_core_finalize();
void argolib_core_kernel(fork_t fptr, void* args);
Task_handle* argolib_core_fork(fork_t fptr, void* args);
void argolib_core_join(Task_handle** list, int size);

#endif
