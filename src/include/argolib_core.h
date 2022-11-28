#ifndef __ARGOLIB_CORE_H__
#define __ARGOLIB_CORE_H__
 
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <abt.h>

typedef ABT_thread Task_handle;
typedef void (*fork_t)(void* args);

// Core argolib API functions
void argolib_core_init(int argc, char ** argv);
void argolib_core_finalize();
void argolib_core_kernel(fork_t fptr, void* args);
Task_handle* argolib_core_fork(fork_t fptr, void* args);
void argolib_core_join(Task_handle** list, int size);

void argolib_core_stop_tracing();
void argolib_core_start_tracing();

#endif
