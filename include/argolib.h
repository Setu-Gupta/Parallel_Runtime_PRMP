#ifndef _ARGOLIB_H
#define _ARGOLIB_H_

#include <abt.h>

typedef ABT_thread Task_handle;
typedef void (*fork_t)(void *args);

// For giving an ID to each thread
typedef struct
{
    void* args; //Pointer to the real arguments to the function
    int tid;    //ID to each thread
} thread_arg_t;

// Global variables
ABT_xstream *xstreams;
ABT_pool *pools;
ABT_sched *scheds;
ABT_thread *threads;
thread_arg_t *thread_args;

// This is just for testing purposes.
#define DEFAULT_NUM_XSTREAMS 2
#define DEFAULT_NUM_THREADS 8

/**
 * Initializes the ArgoLib runtime, and it should be the first thing to call in the user main.
 * Arguments “argc” and “argv” are the ones passed in the call to user main method.
 */
void argolib_init(int argc, char **argv);

/**
 * Finalize the ArgoLib runtime, and performs the cleanup.
 */
void argolib_finalize();

/**
 * User can use this API to launch the top-level computation kernel. This API
 * gathers statistics for the parallel execution and reports after the completion
 * of the computation kernel. Some of the statistics are: execution time, total
 * tasks created, etc. This top-level kernel would actually be launching the recursive tasks.
 */
void argolib_kernel(fork_t fptr, void *args);

/**
 * Creates an Argobot ULT that would execute a user method with the specified argument.
 * It returns a pointer to the task handle that would be used for joining this ULT.
 * *It is the responsibility of the user to create a data structure capable of storing 
 * the created tasks in a recursive program.* Maybe not, threads array has all the created tasks.
 */
Task_handle *argolib_fork(fork_t fptr, void *args);

/**
 * Used for joining one more ULTs using the corresponding task handles. In case of more than one
 * task handles, user can pass an array of Task_handle*. The parameter “size” is the array size.
 * Should have been done using ABT_xstream_join internally. Execution Streams are joint which 
 * result in termination of all Work Units (Tasklet or ULT) in that Execution Stream. 
 * However, the function requires an array of ABT_threads to join. So we need to use ABT_thread_join
 * internally which will join individula threads instead of all the threads in an Execution Stream.
 */
void argolib_join(Task_handle **list, int size);

extern void test(void);

#endif
