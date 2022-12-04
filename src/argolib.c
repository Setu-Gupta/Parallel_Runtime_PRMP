#include <stdio.h>
#include <stdlib.h>
#include <argolib.h>

int num_xstreams;
int num_threads;

int *pool_task;
int *pool_steal;

void argolib_core_init(int argc, char **argv)
{
        argolib_init(argc, argv);
}

void argolib_init(int argc, char **argv)
{
	int is_randws = 0;
	
        char *workers = getenv("ARGOLIB_WORKERS");
        num_xstreams = workers ? atoi(workers) : 1;

        // Minimum size Execution Streams and Threads when taken from user
	if (num_xstreams <= 0)
		num_xstreams = 1;
	if (num_threads <= 0)
		num_threads = 1;

	xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
	pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_xstreams);
	scheds = (ABT_sched *)malloc(sizeof(ABT_sched) * num_xstreams);

        pool_task = (int *) calloc(num_xstreams, sizeof(int));
        pool_steal = (int *) calloc(num_xstreams, sizeof(int));

	threads = (ABT_thread *)malloc(sizeof(ABT_thread) * num_threads);

	ABT_init(argc, argv);

	/* Set up a primary execution stream. */
	ABT_xstream_self(&xstreams[0]);

	/* Create pools. */
	for (int i = 0; i < num_xstreams; i++)
	{
		if (is_randws)
		{
			ABT_pool_create_basic(ABT_POOL_RANDWS, ABT_POOL_ACCESS_MPMC,
								  ABT_TRUE, &pools[i]);
		}
		else
		{
			ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE,
								  &pools[i]);
		}
	}

	/* Create schedulers. */
	for (int i = 0; i < num_xstreams; i++)
	{
		ABT_pool *tmp = (ABT_pool *)malloc(sizeof(ABT_pool) * num_xstreams);
		for (int j = 0; j < num_xstreams; j++)
		{
			tmp[j] = pools[(i + j) % num_xstreams];
		}
		//?Difference between ABT_POOL_RANDWS and ABT_SCHED_RANDWS?
		ABT_sched_create_basic(ABT_SCHED_DEFAULT, num_xstreams, tmp,
							   ABT_SCHED_CONFIG_NULL, &scheds[i]);
		free(tmp);
	}

	// Set the scheduler for the primary execution stream
	ABT_xstream_set_main_sched(xstreams[0], scheds[0]);

	/* Create secondary execution streams. */
	for (int i = 1; i < num_xstreams; i++)
	{
		ABT_xstream_create(scheds[i], &xstreams[i]);
	}
}

Task_handle *argolib_core_fork(fork_t fptr, void *args)
{
        return argolib_fork(fptr, args);
}

Task_handle *argolib_fork(fork_t fptr, void *args)
{
	/** Create ULTs.
	 * The pool associated with this thread is same as the pool of the caller.
	 * thread_pointer will be returned to the caller hence defined static.
	 * Preferably, the caller should pass a thread_arg_t pointer
	 */
	Task_handle *thread_pointer = (Task_handle *)malloc(sizeof(Task_handle));

	int rank;
	ABT_xstream_self_rank(&rank); // Gets the pool index of the calling pool
        pool_task[rank]++;
	ABT_pool target_pool = pools[rank];
	// When should we use ABT_thread_create_to ?
	ABT_thread_create(target_pool, fptr, args,
					  ABT_THREAD_ATTR_NULL, thread_pointer);

	return thread_pointer;
}

void argolib_core_join(Task_handle **list, int size)
{
        argolib_join(list, size);
}

void argolib_join(Task_handle **list, int size)
{
	// First join all the threads
	// ABT_thread_join might not be needed. Confirm!
	for (int i = 0; i < size; i++)
	{
		ABT_thread_join((*list[i]));
	}

	// Free all the threads
	for (int i = 0; i < size; i++)
	{
		ABT_thread_free(list[i]);
	}

	// Free all the thread pointers allocated in fork
	for (int i = 0; i < size; i++)
	{
		free(list[i]);
	}
}

void argolib_core_kernel(fork_t fptr, void *args)
{
        argolib_kernel(fptr, args);
}

void argolib_kernel(fork_t fptr, void *args)
{
        double timeStart = ABT_get_wtime(); // Gives current time in S
	Task_handle *kernel_task[1];
	kernel_task[0] = argolib_fork(fptr, args);

	argolib_join(kernel_task, 1);
        double timeEnd = ABT_get_wtime();

        printf("Execution Time[ms]: %f\n", (timeEnd - timeStart) * 1000.0)
}

void argolib_core_finalize()
{
        argolib_finalize();
}

void argolib_finalize()
{
	// Finalize argobots
	ABT_finalize();

        int totalTask= 0;
        for(int i = 0; i < num_xstreams; i++)
                totalTask += pool_task[i];
        printf("Total Task: %d\n", totalTask);

	// Free allocated memory
	free(xstreams);
	free(pools);
	free(scheds);
	free(threads);
}
