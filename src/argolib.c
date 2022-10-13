// Ref: https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
#include <argolib_core.h>

// Global variables
ABT_xstream *xstreams = NULL;   // Array of handles to execution streams
ABT_pool *pools = NULL;         // Array of handles to task pools
ABT_sched *scheds = NULL;       // Array of handles to schedulers

// This structure represents a deque node
typedef struct unit_t unit_t;
struct unit_t
{
        unit_t *p_prev;
        unit_t *p_next;
        ABT_thread thread;
};

// This structure defines a task pool
typedef struct pool
{
        pthread_mutex_t lock;   // Per pool lock which is used to access the deque
        unit_t *p_head;         // Head of the deque
        unit_t *p_tail;         // Tail of the deque
} pool_t;

// Global variable that holds the number of execution streams being used
int num_xstreams = 0;

unit_t **mailBox = NULL;                // An array of deque nodes which are used for stealing tasks
int *sharedCounter = NULL;              // An array of counters which track the number of tasks in the deque
int *requestBox = NULL;                 // An array of request boxes
pthread_cond_t* cond_vars = NULL;       // An array of condition variables to notify thieves
pthread_mutex_t* cond_mutexes = NULL;   // An array of mutexes to notify thieves

// Various arrays to keep track of the statistics
int *pool_head_push = NULL;
int *pool_head_pop = NULL;
int *pool_tail_push = NULL;
int *pool_tail_pop = NULL;
int *pool_stolen_from = NULL;
int *pool_stole = NULL;

//==================================================== Creating pools for Work Stealing ==========================================

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

        // Pop only if the current thread belongs to this pool
        if(context & ABT_POOL_CONTEXT_OWNER_PRIMARY)
        {
                int rank;
                ABT_xstream_self_rank(&rank);

                // Handle regular deque operations and stealing
                pthread_mutex_lock(&p_pool->lock);
                if (p_pool->p_head == NULL)
                {
                        // Make requests since there are no threads available
                        if (p_pool->p_head == NULL) // If no threads are available
                        {
                                // First Check the Mailbox for a task
                                if (mailBox[rank] != NULL)
                                {
                                        // TODO: Remove
                                        printf("MailBox Non-Empty at %d\n", rank);

                                        // There is a task in Mailbox; pop it
                                        p_unit = mailBox[rank]; // Variable that returns the thread
                                        mailBox[rank] = NULL;   // Empty the Mailbox
                                }
                                else    // Make a request
                                {
                                        bool requestSent = false;
                                        int target = (rank + 1) % num_xstreams;
                                        while (!requestSent && target != rank)
                                        {
                                                // Send request to a Worker with non-empty deque
                                                int num_victim_tasks = __atomic_load_n(&sharedCounter[target], __ATOMIC_SEQ_CST);

                                                if(num_victim_tasks >= 0)
                                                {
                                                        // Put a Steal Request in the Request Box
                                                        unit_t* expected = NULL;
                                                        requestSent = __atomic_compare_exchange_n(&requestBox[target], &expected, rank, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
                                                        if(requestSent)
                                                        {
                                                                pool_stole[rank]++;
                                                                // TODO: Remove
                                                                printf("Request Sent by Worker %d to Worker %d\n", rank, target);

                                                                // Wait for a task
                                                                pthread_mutex_lock(&cond_mutexes[rank]);
                                                                pthread_cond_wait(&cond_vars[rank], &cond_mutexes[rank]);
                                                                p_unit = mailBox[rank];
                                                                pthread_mutex_unlock(&cond_mutexes[rank]);
                                                        }
                                                }
                                                target = (target + 1) % num_xstreams;
                                        } 
                                }

                        }
                }
                else if (p_pool->p_head == p_pool->p_tail)
                {
                        /* Only one thread. */
                        p_unit = p_pool->p_head;
                        p_pool->p_head = NULL;
                        p_pool->p_tail = NULL;
                        pool_head_pop[rank]++;
                        sharedCounter[rank]--;
                }
                else
                {
                        /* Pop from the head. */
                        p_unit = p_pool->p_head;
                        p_pool->p_head = p_unit->p_prev;
                        pool_head_pop[rank]++;
                        __atomic_sub_fetch(&sharedCounter[rank], 1, __ATOMIC_SEQ_CST);
                }
                pthread_mutex_unlock(&p_pool->lock);
        }

        if (!p_unit)
                return ABT_THREAD_NULL;

        return p_unit->thread;
}

static void pool_push(ABT_pool pool, ABT_unit unit, ABT_pool_context context)
{
        pool_t *p_pool;
        ABT_pool_get_data(pool, (void **)&p_pool);
        unit_t *p_unit = (unit_t *)unit;

        int rank;
        ABT_xstream_self_rank(&rank);

        // Lock the pool
        pthread_mutex_lock(&p_pool->lock);
        
        // Fullfill requests
        int requesterRank = __atomic_load_n(&requestBox[rank], __ATOMIC_SEQ_CST);
        if (requesterRank != -1)        // If there is a request available
        {
                // TODO: Remove after debugging
                printf("Valid Request\n");

                // Pop from the tail and try to assign the task to the requester
                p_unit = p_pool->p_tail;
                p_pool->p_tail = p_unit->p_next;

                // Assign the task and signal the requester
                pthread_mutex_lock(&cond_mutexes[requesterRank]);
                mailBox[requesterRank] = p_unit;
                pthread_cond_signal(&cond_vars[requesterRank]);
                pthread_mutex_unlock(&cond_mutexes[requesterRank]);

                pool_stolen_from[rank]++;
                pool_tail_pop[rank]++;
                sharedCounter[rank]--;  // Decrement the counter as one job was popped
                requestBox[rank] = -1;   // Clear The request box
        }

        // Perform regular deque function, i.e. push to the head
        if (p_pool->p_head)    // If pool is not empty
                p_unit->p_prev = p_pool->p_head;
        p_pool->p_head = p_unit;
        
        pool_head_push[rank]++;
        pthread_mutex_unlock(&p_pool->lock);
        
        __atomic_add_fetch(&sharedCounter[rank], 1, __ATOMIC_SEQ_CST);
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

//==================================================== Creating schedulers for Work Stealing ==========================================

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

//==================================================== Argolib APIs ===================================================================
/*
 * Prints the statistics
 */
static void _print_stats()
{
        printf("=================== STATISTICS =======================\n");
        for (int i = 0; i < num_xstreams; i++)
        {
                printf("Pool %d\n", i);
                printf("\tPush Head: %d\tPush Tail: %d\n", pool_head_push[i], pool_tail_push[i]);
                printf("\tPop Head: %d\tPop Tail: %d\n", pool_head_pop[i], pool_tail_pop[i]);
                printf("\tNumber of stolen tasks: %d\n", pool_stole[i]);
                printf("\tNumber of tasks donated: %d\n", pool_stolen_from[i]);
        }
        printf("=====================================================\n");
}

/*
 * Initializes the argobots runtime, the schedulers and the pools.
 */
void argolib_core_init(int argc, char **argv)
{
        char *workers = getenv("ARGOLIB_WORKERS");
        char *randomws = getenv("ARGOLIB_RANDOMWS");

        // Get the number of execution streams to spawn from the environment
        num_xstreams = workers ? atoi(workers) : 1;
        if (num_xstreams <= 0)
                num_xstreams = 1;
        bool is_randws = randomws ? (atoi(randomws) > 0 ? 1 : 0) : 0;

        // Allocate memory for statistics
        pool_head_push = (int *)calloc(num_xstreams, sizeof(int));
        pool_head_pop = (int *)calloc(num_xstreams, sizeof(int));
        pool_tail_push = (int *)calloc(num_xstreams, sizeof(int));
        pool_tail_pop = (int *)calloc(num_xstreams, sizeof(int));
        pool_stolen_from = (int *)calloc(num_xstreams, sizeof(int));
        pool_stole = (int *)calloc(num_xstreams, sizeof(int));
        if(!pool_head_push || !pool_head_pop || !pool_tail_push || !pool_tail_pop || !pool_stolen_from || !pool_stole)
        {
                fprintf(stderr, "[ERORR] Could not allocate memory for statistics. Exiting...\n");
                exit(-1);
        }

        // Allocate memory for argobots objects
        xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
        pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_xstreams);
        scheds = (ABT_sched *)malloc(sizeof(ABT_sched) * num_xstreams);
        mailBox = (unit_t **)malloc(sizeof(unit_t*) * num_xstreams);
        sharedCounter = (int *)calloc(num_xstreams, sizeof(int));
        requestBox = (int *)calloc(num_xstreams, sizeof(int));
        cond_vars = (pthread_cond_t *)calloc(num_xstreams, sizeof(pthread_cond_t));
        cond_mutexes = (pthread_mutex_t*)calloc(num_xstreams, sizeof(pthread_mutex_t));
        if(!xstreams || !pools || !scheds || !mailBox || !sharedCounter || !requestBox || !cond_vars || !cond_mutexes)
        {
                fprintf(stderr, "[ERORR] Could not allocate memory for argobots objects. Exiting...\n");
                exit(-1);
        }

        // Initialize mailBoxes, requestBoxes and conditional variables
        for (int i = 0; i < num_xstreams; i++)
        {
                requestBox[i] = -1;     // Initialize Request Box IDs with -1
                mailBox[i] = NULL;
                if(pthread_cond_init(&cond_vars[i], NULL) != 0)
                {
                        fprintf(stderr, "[ERORR] Could not initialize conditional variable. Exiting...\n");
                        exit(-1);
                }
                if(pthread_mutex_init(&cond_mutexes[i], NULL) != 0)
                {
                        fprintf(stderr, "[ERORR] Could not initialize mutex. Exiting...\n");
                        exit(-1);
                }
        }

        ABT_init(argc, argv);

        // Set the first xstream handle to the current xstream
        ABT_xstream_self(&xstreams[0]);

        // Create pools
        if (is_randws)
                create_pools(num_xstreams, pools);      // Create custom pools
        else    // Create default pools
                for (int i = 0; i < num_xstreams; i++)
                        ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &pools[i]);

        // Create schedulers
        if (is_randws)
                create_scheds(num_xstreams, pools, scheds);     // Create custom schedulers
        else
        {
                for (int i = 0; i < num_xstreams; i++)
                {
                        ABT_pool *tmp = (ABT_pool *)malloc(sizeof(ABT_pool) * num_xstreams);

                        // tmp[0] is the main pool and the rest are the pools from which tasks can be stolen from
                        for (int j = 0; j < num_xstreams; j++)
                                tmp[j] = pools[(i + j) % num_xstreams];

                        ABT_sched_create_basic(ABT_SCHED_DEFAULT, num_xstreams, tmp, ABT_SCHED_CONFIG_NULL, &scheds[i]);
                        free(tmp);
                }
        }

        // Set the scheduler for the primary execution stream
        ABT_xstream_set_main_sched(xstreams[0], scheds[0]);

        // Create secondary execution streams and assign schedulers to them
        for (int i = 1; i < num_xstreams; i++)
                ABT_xstream_create(scheds[i], &xstreams[i]);
}

/*
 * Finalizes the argobots runtime and frees variables
 */
void argolib_core_finalize()
{
        // Wait for all execution streams to finish
        // Free execution streams after they are finished
        for (int i = 1; i < num_xstreams; i++)
        {
                ABT_xstream_join(xstreams[i]);
                ABT_xstream_free(&xstreams[i]);
        }

        // Freeing all the schedulers
        for (int i = 1; i < num_xstreams; i++)
                ABT_sched_free(&scheds[i]);

        // Destroy conditional variables
        for (int i = 1; i < num_xstreams; i++)
        {
                pthread_mutex_destroy(&cond_mutexes[i]);
                pthread_cond_destroy(&cond_vars[i]);
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
        free(cond_vars);
        free(cond_mutexes);
        free(pool_head_push);
        free(pool_head_pop);
        free(pool_tail_push);
        free(pool_tail_pop);
        free(pool_stolen_from);
        free(pool_stole);
}

/*
 * Forks a new task to run fptr with args
 */
Task_handle *argolib_core_fork(fork_t fptr, void *args)
{
        Task_handle *thread_pointer = (Task_handle *)malloc(sizeof(Task_handle));
        int rank;
        ABT_xstream_self_rank(&rank);   // Gets the pool index of the calling pool
        ABT_pool target_pool = pools[rank];
        ABT_thread_create(target_pool, fptr, args, ABT_THREAD_ATTR_NULL, thread_pointer);       // Create a new thread and push it into the current pool
        return thread_pointer;
}

/*
 * Waits for all the Task_handles to finish
 */
void argolib_core_join(Task_handle **list, int size)
{
        // First join all the threads
        for (int i = 0; i < size; i++)
                ABT_thread_join((*list[i]));

        // Free all the threads
        for (int i = 0; i < size; i++)
                ABT_thread_free(list[i]);

        // Free all the thread pointers allocated in fork
        for (int i = 0; i < size; i++)
                free(list[i]);
}

/*
 * Executes a computation kernel
 */
void argolib_core_kernel(fork_t fptr, void *args)
{
        double timeStart = ABT_get_wtime(); // Gives current time
        fptr(args);
        double timeEnd = ABT_get_wtime();
        printf("Execution Time[ms]: %f\n", (timeEnd - timeStart) * 1000.0);
        _print_stats();
}
