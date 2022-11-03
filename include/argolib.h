#ifndef __ARGOLIB_H__
#define __ARGOLIB_H__

#include "./../src/include/argolib_core.h"

/**
 * Initializes the ArgoLib runtime, and it should be the first thing to call in the user main.
 * Arguments “argc” and “argv” are the ones passed in the call to user main method.
 */
void argolib_init(int argc, char **argv)
{
        argolib_core_init(argc, argv);
}

/**
 * Finalize the ArgoLib runtime, and performs the cleanup.
 */
void argolib_finalize()
{
        argolib_core_finalize();
}

/**
 * User can use this API to launch the top-level computation kernel. This API
 * gathers statistics for the parallel execution and reports after the completion
 * of the computation kernel. Some of the statistics are: execution time, total
 * tasks created, etc. This top-level kernel would actually be launching the recursive tasks.
 */
void argolib_kernel(fork_t fptr, void *args)
{
        argolib_core_kernel(fptr, args);
}

/**
 * Creates an Argobot ULT that would execute a user method with the specified argument.
 * It returns a pointer to the task handle that would be used for joining this ULT.
 * *It is the responsibility of the user to create a data structure capable of storing 
 * the created tasks in a recursive program.* Maybe not, threads array has all the created tasks.
 */
Task_handle *argolib_fork(fork_t fptr, void *args)
{
        return argolib_core_fork(fptr, args);
}

/**
 * Used for joining one more ULTs using the corresponding task handles. In case of more than one
 * task handles, user can pass an array of Task_handle*. The parameter “size” is the array size.
 * Should have been done using ABT_xstream_join internally. Execution Streams are joint which 
 * result in termination of all Work Units (Tasklet or ULT) in that Execution Stream. 
 * However, the function requires an array of ABT_threads to join. So we need to use ABT_thread_join
 * internally which will join individula threads instead of all the threads in an Execution Stream.
 */
void argolib_join(Task_handle **list, int size)
{
        argolib_core_join(list, size);
}

/**
 * Used to start the trace collection for a compute kernel. This can be called multiple times.
 * To use trace and replay optimization, the user should call the kernel in the following manner:
 * loop
 * {
 *      start_tracing();
 *      kernel();
 *      stop_tracing();
 * }
 */
void argolib_start_tracing()
{
        void argolib_core_start_tracing();
}

/* Used to stop trace collection
*/
void argolib_stop_tracing()
{
        void argolib_core_stop_tracing();
}

#endif
