#include <argolib_core.h>

// Global variables
ABT_xstream *xstreams;
ABT_pool *pools;
ABT_sched *scheds;
thread_arg_t *thread_args;

static void create_pools(int num, ABT_pool *pools);
static void create_scheds(int num, ABT_pool *pools, ABT_sched *scheds);

/** Creating Pool for Work Stealing Runtime
 *
 */

typedef struct unit_t unit_t;
typedef struct pool_t pool_t;

struct unit_t
{
        unit_t *p_prev;
        unit_t *p_next;
        ABT_thread thread;
};

struct pool_t
{
        pthread_mutex_t lock;
        unit_t *p_head;
        unit_t *p_tail;
};

int num_xstreams;

unit_t **mailBox;
int *sharedCounter;
int *requestBox;

pthread_mutex_t pplock;
int net_push = 0;
int net_pop = 0;

int *pool_net_push;
int *pool_net_pop;
int *pool_head_push;
int *pool_head_pop;
int *pool_tail_push;
int *pool_tail_pop;
int *pool_stolen_from;
int *pool_stole_from;

int *mailBox_task;

void print_stats()
{

        for (int i = 0; i < num_xstreams; i++)
        {
                printf("Pool %d\n", i);
                printf("\tPush Head: %d\tPush Tail: %d\n", pool_head_push[i], pool_tail_push[i]);
                printf("\tPop Head: %d\tPop Tail: %d\n", pool_head_pop[i], pool_tail_pop[i]);
                printf("\tStolen From: %d\n", pool_stolen_from[i]);
                printf("\tPush: %d\tPop: %d\n", pool_net_push[i], pool_net_pop[i]);
        }

        printf("\n");
        printf("Net pushes: %d\n", net_push);
        printf("Net pops: %d\n", net_pop);
}

void argolib_core_init(int argc, char **argv)
{
        char *workers = getenv("ARGOLIB_WORKERS");
        char *randomws = getenv("ARGOLIB_RANDOMWS");

        num_xstreams = workers ? atoi(workers) : 1;
        bool is_randws = randomws ? (atoi(randomws) > 0 ? 1 : 0) : 0;

        pthread_mutex_init(&pplock, 0);
        pool_net_push = (int *)calloc(num_xstreams, sizeof(int));
        pool_net_pop = (int *)calloc(num_xstreams, sizeof(int));
        pool_head_push = (int *)calloc(num_xstreams, sizeof(int));
        pool_head_pop = (int *)calloc(num_xstreams, sizeof(int));
        pool_tail_push = (int *)calloc(num_xstreams, sizeof(int));
        pool_tail_pop = (int *)calloc(num_xstreams, sizeof(int));
        pool_stolen_from = (int *)calloc(num_xstreams, sizeof(int));
        pool_stole_from = (int *)calloc(num_xstreams, sizeof(int));

        mailBox_task = (int *)calloc(num_xstreams, sizeof(int));

        // Minimum size Execution Streams and Threads when taken from user
        if (num_xstreams <= 0)
                num_xstreams = 1;

        xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
        pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_xstreams);
        scheds = (ABT_sched *)malloc(sizeof(ABT_sched) * num_xstreams);

        mailBox = (unit_t **)malloc(sizeof(unit_t*) * num_xstreams);
        sharedCounter = (int *)calloc(num_xstreams, sizeof(int));
        requestBox = (int *)calloc(num_xstreams, sizeof(int));

        for (int i = 0; i < num_xstreams; i++)
        {
                requestBox[i] = -1; // Initialize Request Box IDs with -1
                mailBox[i] = NULL;
        }

        ABT_init(argc, argv);

        /* Set up a primary execution stream. */
        ABT_xstream_self(&xstreams[0]);

        /* Create pools. */

        if (is_randws)
                create_pools(num_xstreams, pools);
        else
        {
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
        }

        /* Create schedulers. */
        if (is_randws)
                create_scheds(num_xstreams, pools, scheds);
        else
        {
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
        /** Create ULTs.
         * The pool associated with this thread is same as the pool of the caller.
         * thread_pointer will be returned to the caller hence defined static.
         * Preferably, the caller should pass a thread_arg_t pointer
         */
        Task_handle *thread_pointer = (Task_handle *)malloc(sizeof(Task_handle));

        int rank;
        ABT_xstream_self_rank(&rank); // Gets the pool index of the calling pool
        ABT_pool target_pool = pools[rank];
        // printf("Forked from ES %d\n", rank);
        //  When should we use ABT_thread_create_to ?
        //  This internally pushes the thread into the pool
        ABT_thread_create(target_pool, fptr, args,
                          ABT_THREAD_ATTR_NULL, thread_pointer);

        return thread_pointer;
}

void argolib_core_join(Task_handle **list, int size)
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
        // Task_handle *kernel_task[1];
        // kernel_task[0] = argolib_core_fork(fptr, args);
        // argolib_core_join(kernel_task, 1);
        double timeStart = ABT_get_wtime(); // Gives current time in S
        fptr(args);
        double timeEnd = ABT_get_wtime();

        printf("Execution Time[ms]: %f\n", (timeEnd - timeStart) * 1000.0);

        print_stats();
}

void argolib_core_finalize()
{
        // Waiting for all Execution Streams to finish
        // Freeing Execution streams after they are finished
        for (int i = 1; i < num_xstreams; i++)
        {
                ABT_xstream_join(xstreams[i]);
                ABT_xstream_free(&xstreams[i]);
        }

        // Freeing all the schedulers
        for (int i = 1; i < num_xstreams; i++)
        {
                ABT_sched_free(&scheds[i]);
        }
        // Finalize argobots
        ABT_finalize();

        // Free allocated memory
        free(xstreams);
        free(pools);
        free(scheds);

        free(mailBox);
        free(requestBox);
        free(sharedCounter);

        free(pool_head_push);
        free(pool_head_pop);
        free(pool_tail_push);
        free(pool_tail_pop);
        free(pool_stolen_from);
        free(pool_stole_from);
}

// Custom Work Stealing
/* Pool functions */
static ABT_unit pool_create_unit(ABT_pool pool, ABT_thread thread)
{
        unit_t *p_unit = (unit_t *)calloc(1, sizeof(unit_t));
        if (!p_unit)
                return ABT_UNIT_NULL;
        p_unit->thread = thread;
        return (ABT_unit)p_unit;
}

static void pool_free_unit(ABT_pool pool, ABT_unit unit)
{
        unit_t *p_unit = (unit_t *)unit;
        free(p_unit);
}

static ABT_bool pool_is_empty(ABT_pool pool)
{
        pool_t *p_pool;
        ABT_pool_get_data(pool, (void **)&p_pool);
        return p_pool->p_head ? ABT_FALSE : ABT_TRUE;
}

static ABT_thread pool_pop(ABT_pool pool, ABT_pool_context context)
{
        pool_t *p_pool;
        ABT_pool_get_data(pool, (void **)&p_pool);
        unit_t *p_unit = NULL;

        int rank;
        ABT_xstream_self_rank(&rank);

        unsigned seed = time(NULL);
        int target;

        bool isValidRequest = false;
        int requesterRank;

        // Always First check if there is a request in the Request Box
        pthread_mutex_lock(&p_pool->lock);
        isValidRequest = requestBox[rank] != -1;
        pthread_mutex_unlock(&p_pool->lock);

        if (isValidRequest)
        {
                printf("Valid Request\n");
                requesterRank = requestBox[rank];
                // There is a request in the Request Box
                // Pop from the Tail
                p_unit = p_pool->p_tail;
                p_pool->p_tail = p_unit->p_next;
                pool_tail_pop[rank]++;
                pool_stolen_from[rank]++;

                pthread_mutex_lock(&p_pool->lock);
                sharedCounter[rank]--;
                mailBox[requesterRank] = p_unit;
                pthread_mutex_unlock(&p_pool->lock);
        }

        // pthread_mutex_lock(&p_pool->lock);
        if (p_pool->p_head == NULL)
        {
                /* Empty. */
                // First Check the Mailbox for a task
                if (mailBox[rank] != NULL)
                {
                        printf("MailBox Non-Empty at %d\n", rank);
                        // Take Mutex on Mailbox as it can be Written by the Victim as well
                        pthread_mutex_lock(&p_pool->lock);
                        {
                                // There is a task in Mailbox; pop it
                                p_unit = mailBox[rank]; // Variable that returns the thread
                                mailBox[rank] = NULL;   // Empty the Mailbox
                                mailBox_task[rank]++;
                        }
                        pthread_mutex_unlock(&p_pool->lock);
                }
                else
                {
                        bool requestSent = false;

                        while (!requestSent)
                        {
                                // Both Deque and Mailbox are empty
                                // Send request to a Worker with non-empty deque
                                target = (num_xstreams == 2) ? 1 : (rand_r(&seed) % (num_xstreams - 1) + 1);

                                if (sharedCounter[target] >= 2)
                                {
                                        // Take lock on Request Box as it is read by the Victim as well
                                        pthread_mutex_lock(&p_pool->lock);
                                        // Put a Steal Request in the Request Box
                                        requestBox[target] = rank;
                                        pthread_mutex_unlock(&p_pool->lock);

                                        requestSent = true;
                                }
                        } // Potential Deadlock if only 2 ES and Stealer is empty and the other worker has only 1 or 0 threads
                }
        }
        else if (p_pool->p_head == p_pool->p_tail)
        {
                { /* Only one thread. */
                        p_unit = p_pool->p_head;
                        p_pool->p_head = NULL;
                        p_pool->p_tail = NULL;
                        pool_head_pop[rank]++;
                        pthread_mutex_lock(&p_pool->lock);
                        sharedCounter[rank]--;
                        pthread_mutex_unlock(&p_pool->lock);
                }
        }
        // else if (context & ABT_POOL_CONTEXT_OWNER_SECONDARY)
        // {
        //         { /* Pop from the tail. */
        //                 p_unit = p_pool->p_tail;
        //                 p_pool->p_tail = p_unit->p_next;
        //                 pool_tail_pop[rank]++;
        //                 pool_stolen_from[rank]++;
        //         }
        // }
        else
        {
                /* Pop from the head. */
                p_unit = p_pool->p_head;
                p_pool->p_head = p_unit->p_prev;
                pool_head_pop[rank]++;
                pthread_mutex_lock(&p_pool->lock);
                sharedCounter[rank]--;
                pthread_mutex_unlock(&p_pool->lock);
        }
        // pthread_mutex_unlock(&p_pool->lock);
        if (!p_unit)
                return ABT_THREAD_NULL;
        pthread_mutex_lock(&pplock);
        net_pop++;
        pool_net_pop[rank]++;
        pthread_mutex_unlock(&pplock);
        return p_unit->thread;
}

static void pool_push(ABT_pool pool, ABT_unit unit, ABT_pool_context context)
{
        pool_t *p_pool;
        ABT_pool_get_data(pool, (void **)&p_pool);
        unit_t *p_unit = (unit_t *)unit;

        int rank;
        ABT_xstream_self_rank(&rank);

        pthread_mutex_lock(&p_pool->lock);
        pthread_mutex_lock(&pplock);
        net_push++;
        pool_net_push[rank]++;
        pthread_mutex_unlock(&pplock);
        if (context & (ABT_POOL_CONTEXT_OP_THREAD_CREATE |
                       ABT_POOL_CONTEXT_OP_THREAD_CREATE_TO |
                       ABT_POOL_CONTEXT_OP_THREAD_REVIVE |
                       ABT_POOL_CONTEXT_OP_THREAD_REVIVE_TO))
        {
                /* Push to the head. */
                if (p_pool->p_head)
                {
                        p_unit->p_prev = p_pool->p_head;
                        p_pool->p_head->p_next = p_unit;
                }
                else
                {
                        p_pool->p_tail = p_unit;
                }
                p_pool->p_head = p_unit;
                pool_head_push[rank]++;
        }
        else
        {
                /* Push to the tail. */
                if (p_pool->p_tail)
                {
                        // Pool is non-empty
                        p_unit->p_next = p_pool->p_tail;
                        p_pool->p_tail->p_prev = p_unit;
                }
                else
                {
                        // Pool is empty
                        p_pool->p_head = p_unit;
                }
                p_pool->p_tail = p_unit;
                pool_tail_push[rank]++;
        }
        sharedCounter[rank]++;
        pthread_mutex_unlock(&p_pool->lock);
}

static int pool_init(ABT_pool pool, ABT_pool_config config)
{
        pool_t *p_pool = (pool_t *)calloc(1, sizeof(pool_t));
        if (!p_pool)
                return ABT_ERR_MEM;

        /* Initialize the spinlock */
        int ret = pthread_mutex_init(&p_pool->lock, 0);
        if (ret != 0)
        {
                free(p_pool);
                return ABT_ERR_SYS;
        }
        ABT_pool_set_data(pool, (void *)p_pool);
        return ABT_SUCCESS;
}

static void pool_free(ABT_pool pool)
{
        pool_t *p_pool;
        ABT_pool_get_data(pool, (void **)&p_pool);
        pthread_mutex_destroy(&p_pool->lock);
        free(p_pool);
}

static void create_pools(int num, ABT_pool *pools)
{
        /* Pool definition */
        ABT_pool_user_def def;
        ABT_pool_user_def_create(pool_create_unit, pool_free_unit, pool_is_empty,
                                 pool_pop, pool_push, &def);
        ABT_pool_user_def_set_init(def, pool_init);
        ABT_pool_user_def_set_free(def, pool_free);
        /* Pool configuration */
        ABT_pool_config config;
        ABT_pool_config_create(&config);
        /* The same as a pool created by ABT_pool_create_basic(). */
        const int automatic = 1;
        ABT_pool_config_set(config, ABT_pool_config_automatic.key,
                            ABT_pool_config_automatic.type, &automatic);

        int i;
        for (i = 0; i < num; i++)
        {
                ABT_pool_create(def, config, &pools[i]);
        }
        ABT_pool_user_def_free(&def);
        ABT_pool_config_free(&config);
}

/** Creating Scheduler for Work Stealing
 */

typedef struct
{
        uint32_t event_freq;
} sched_data_t;

static int sched_init(ABT_sched sched, ABT_sched_config config)
{
        sched_data_t *p_data = (sched_data_t *)calloc(1, sizeof(sched_data_t));

        ABT_sched_config_read(config, 1, &p_data->event_freq);
        ABT_sched_set_data(sched, (void *)p_data);

        return ABT_SUCCESS;
}

static void sched_run(ABT_sched sched)
{
        uint32_t work_count = 0;
        sched_data_t *p_data;
        int num_pools;
        ABT_pool *pools;
        // int target;
        ABT_bool stop;
        // unsigned seed = time(NULL);

        ABT_sched_get_data(sched, (void **)&p_data);
        ABT_sched_get_num_pools(sched, &num_pools);
        pools = (ABT_pool *)malloc(num_pools * sizeof(ABT_pool));
        ABT_sched_get_pools(sched, num_pools, 0, pools);

        while (1)
        {
                /* Execute one work unit from the scheduler's pool */
                ABT_thread thread;
                ABT_pool_pop_thread_ex(pools[0], &thread, ABT_POOL_CONTEXT_OWNER_PRIMARY);
                if (thread != ABT_THREAD_NULL)
                {
                        /* "thread" is associated with its original pool (pools[0]). */
                        ABT_self_schedule(thread, ABT_POOL_NULL);
                }

                // If thread == ABT_THREAD_NULL, then the main pool must have requested
                // another pool for a steal. Now we need to wait for the other pool to serve
                // the request and eventually, we would have a task after it has served our request.

                // else if (num_pools > 1)
                // {
                //         /* Steal a work unit from other pools */
                //         target =
                //             (num_pools == 2) ? 1 : (rand_r(&seed) % (num_pools - 1) + 1);
                //         ABT_pool_pop_thread_ex(pools[target], &thread, ABT_POOL_CONTEXT_OWNER_SECONDARY);
                //         if (thread != ABT_THREAD_NULL)
                //         {
                //                 /* "thread" is associated with its original pool
                //                  * (pools[target]). */
                //                 ABT_self_schedule(thread, pools[target]);
                //         }
                // }

                if (++work_count >= p_data->event_freq)
                {
                        work_count = 0;
                        ABT_sched_has_to_stop(sched, &stop);
                        if (stop == ABT_TRUE)
                                break;
                        ABT_xstream_check_events(sched);
                }
        }

        free(pools);
}

static int sched_free(ABT_sched sched)
{
        sched_data_t *p_data;

        ABT_sched_get_data(sched, (void **)&p_data);
        free(p_data);

        return ABT_SUCCESS;
}

static void create_scheds(int num, ABT_pool *pools, ABT_sched *scheds)
{
        ABT_sched_config config;
        ABT_pool *my_pools;
        int i, k;

        ABT_sched_config_var cv_event_freq = {.idx = 0,
                                              .type = ABT_SCHED_CONFIG_INT};

        ABT_sched_def sched_def = {.type = ABT_SCHED_TYPE_ULT,
                                   .init = sched_init,
                                   .run = sched_run,
                                   .free = sched_free,
                                   .get_migr_pool = NULL};

        /* Create a scheduler config */
        ABT_sched_config_create(&config, cv_event_freq, 10,
                                ABT_sched_config_var_end);

        my_pools = (ABT_pool *)malloc(num * sizeof(ABT_pool));
        for (i = 0; i < num; i++)
        {
                for (k = 0; k < num; k++)
                {
                        my_pools[k] = pools[(i + k) % num];
                }

                ABT_sched_create(&sched_def, num, my_pools, config, &scheds[i]);
        }
        free(my_pools);

        ABT_sched_config_free(&config);
}
