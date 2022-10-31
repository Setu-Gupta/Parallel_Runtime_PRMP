// Ref: https://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html

#include <argolib_core.h>
#include <assert.h>

// Global variables
ABT_xstream *xstreams;  // Array of execution streams
ABT_pool *pools;        // Array of thread pools
ABT_sched *scheds;      // Array of schedulers

static void create_pools(int num, ABT_pool *pools);
static void create_scheds(int num, ABT_pool *pools, ABT_sched *scheds);

typedef struct unit_t unit_t;
typedef struct pool_t pool_t;

struct unit_t
{
        unit_t *p_prev;         // Pointer to previous unit in the dequeue
        unit_t *p_next;         // Pointer to next unit in the dequeue
        ABT_thread thread;      // Thread to be executed
};

struct pool_t
{
        pthread_mutex_t lock;   // Pool specific lock to ensure one one xstream can access it at once
        unit_t *p_head;         // Head of the dequeue
        unit_t *p_tail;         // Tail of the dequeue
};

int num_xstreams;
bool trace_enabled;     // Decides whether to use trace-replay or not

int *sharedCounter;     // Counter which tracks how many tasks are available in dequeues
int *requestBox;        // Request-box for every worker
unit_t **mailBox;       // Mailbox for every worker
pthread_cond_t *cond_vars;     // Condition variable to signal the stealer when task is supplied

// TODO: Remove; Used for polling
bool *requestSent;
bool *requestServed;

pthread_mutex_t pplock; // TODO: See what it is used for

int *pool_net_push;
int *pool_net_pop;
int *pool_head_push;
int *pool_head_pop;
int *pool_tail_push;
int *pool_tail_pop;
int *pool_stolen_from;
int *pool_stole_from;
int *pool_tasks_created;

void print_stats()
{
        int net_head_push = 0;
        int net_tail_push = 0;
        int net_head_pops = 0;
        int net_tail_pops = 0;
        int net_stolen_from = 0;
        int net_stole_from = 0;
        int net_tasks = 0;

        for (int i = 0; i < num_xstreams; i++)
        {
                printf("Pool %d\n", i);
                printf("\tPush Head: %d\tPush Tail: %d\n", pool_head_push[i], pool_tail_push[i]);
                printf("\tPop Head: %d\tPop Tail: %d\n", pool_head_pop[i], pool_tail_pop[i]);
                printf("\tTasks Stolen From %d: %d\n", i, pool_stolen_from[i]);
                printf("\tTasks %d Stole: %d\n", i, pool_stole_from[i]);
                printf("\tPush: %d\tPop: %d\n", pool_net_push[i], pool_net_pop[i]);
                net_head_push += pool_head_push[i];
                net_tail_push += pool_tail_push[i];
                net_head_pops += pool_head_pop[i];
                net_tail_pops += pool_tail_pop[i];
                net_stolen_from += pool_stolen_from[i];
                net_stole_from += pool_stole_from[i];
                net_tasks += pool_tasks_created[i];
        }

        printf("\n");
        printf("Net head pushes: %d\tNet tail pushes: %d\n", net_head_push, net_tail_push);
        printf("Net head pops: %d\tNet tail pops: %d\n", net_head_pops, net_tail_pops);
        printf("Total Tasks Created: %d\n", net_tasks);
        printf("Total tasks stolen: %d\n", net_stolen_from);
        printf("Total tasks stolen: %d\n", net_stolen_from);
}

void argolib_core_init(int argc, char **argv)
{
        char *workers = getenv("ARGOLIB_WORKERS");
        char *randomws = getenv("ARGOLIB_RANDOMWS");
        char *trace = getenv("ARGOLIB_TRACE");

        num_xstreams = workers ? atoi(workers) : 1;                     // Number of workers to spawn
        bool is_randws = randomws ? (atoi(randomws) > 0 ? 1 : 0) : 0;   // Used to decide whether to do random work stealing or not
        trace_enabled = trace ? (atoi(trace) > 0 ? 1 : 0) : 0;          // Used to decide whether to use trace replay optimization or not

        pthread_mutex_init(&pplock, 0); // TODO: Remove if not needed
        pool_net_push = (int *)calloc(num_xstreams, sizeof(int));
        pool_net_pop = (int *)calloc(num_xstreams, sizeof(int));
        pool_head_push = (int *)calloc(num_xstreams, sizeof(int));
        pool_head_pop = (int *)calloc(num_xstreams, sizeof(int));
        pool_tail_push = (int *)calloc(num_xstreams, sizeof(int));
        pool_tail_pop = (int *)calloc(num_xstreams, sizeof(int));
        pool_stolen_from = (int *)calloc(num_xstreams, sizeof(int));
        pool_stole_from = (int *)calloc(num_xstreams, sizeof(int));
        pool_tasks_created = (int *)calloc(num_xstreams, sizeof(int));
        cond_vars = (pthread_cond_t*)calloc(num_xstreams, sizeof(pthread_cond_t));

        // Minimum number of execution streams should be at least one
        if (num_xstreams <= 0)
                num_xstreams = 1;

        xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
        pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_xstreams);
        scheds = (ABT_sched *)malloc(sizeof(ABT_sched) * num_xstreams);

        mailBox = (unit_t **)malloc(sizeof(unit_t*) * num_xstreams);
        sharedCounter = (int *)calloc(num_xstreams, sizeof(int));
        requestBox = (int *)calloc(num_xstreams, sizeof(int));

        // TODO: Remove; used for polling
        requestSent = (bool *)calloc(num_xstreams, sizeof(bool));
        requestServed = (bool *)calloc(num_xstreams, sizeof(bool));
        
        // Initialize the state for each worker
        for (int i = 0; i < num_xstreams; i++)
        {
                requestBox[i] = -1;     // Initialize Request Box IDs with -1 i.e. no request
                mailBox[i] = NULL;      // Initialize mailbox to null indicating no available tasks
                pthread_cond_init(&cond_vars[i], NULL);
                
                // TODO: Remove; used for polling
                requestSent[i] = false;
                requestServed[i] = false;
        }

        ABT_init(argc, argv);

        // Set up a primary execution stream
        ABT_xstream_self(&xstreams[0]);

        // Create pools
        if (is_randws)
                create_pools(num_xstreams, pools);
        else
                for (int i = 0; i < num_xstreams; i++)
                        ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &pools[i]);

        // Create schedulers
        if (is_randws)
                create_scheds(num_xstreams, pools, scheds);
        else
        {
                for (int i = 0; i < num_xstreams; i++)
                {
                        ABT_pool *temp = (ABT_pool *)malloc(sizeof(ABT_pool) * num_xstreams);
                        for (int j = 0; j < num_xstreams; j++)
                                temp[j] = pools[(j + i) % num_xstreams]; // temp[0] is the primary pool for the scheduler
                        ABT_sched_create_basic(ABT_SCHED_DEFAULT, num_xstreams, temp, ABT_SCHED_CONFIG_NULL, &scheds[i]);
                        free(temp);
                }
        }

        // Set the scheduler for the primary execution stream
        ABT_xstream_set_main_sched(xstreams[0], scheds[0]);

        // Create secondary execution streams and set their schedulers
        for (int i = 1; i < num_xstreams; i++)
                ABT_xstream_create(scheds[i], &xstreams[i]);
}

Task_handle *argolib_core_fork(fork_t fptr, void *args)
{
        // Create a task handle for the thread which is supposed to execute fptr(args)
        Task_handle *thread_pointer = (Task_handle *)malloc(sizeof(Task_handle));

        int rank;                               // ID of the worker who called fork
        ABT_xstream_self_rank(&rank);           // Gets the primary pool index of the calling worker
        ABT_pool target_pool = pools[rank];     // Get the pointer to the pool in which this task has to be pushed
        ABT_thread_create(target_pool, fptr, args, ABT_THREAD_ATTR_NULL, thread_pointer);       // Create a thread to execute fptr(args) and push it to target_pool
        pool_tasks_created[rank]++;
        return thread_pointer;
}

void argolib_core_join(Task_handle **list, int size)
{
        // Join and free all the threads
        for (int i = 0; i < size; i++)
        {
                ABT_thread_join(*list[i]);
                ABT_thread_free(list[i]);
        }

        // Free all the thread pointers allocated in fork
        for (int i = 0; i < size; i++)
                free(list[i]);
}

void argolib_core_kernel(fork_t fptr, void *args)
{
        double timeStart = ABT_get_wtime(); // Gives current time in seconds
        fptr(args);
        double timeEnd = ABT_get_wtime();

        printf("Execution Time[ms]: %f\n", (timeEnd - timeStart) * 1000.0);
        print_stats();
}

void argolib_core_finalize()
{
        // Joining and freeing xstreams after they are finished
        for (int i = 1; i < num_xstreams; i++)
        {
                ABT_xstream_join(xstreams[i]);
                ABT_xstream_free(&xstreams[i]);
        }

        // Freeing all the schedulers
        for (int i = 1; i < num_xstreams; i++)
                ABT_sched_free(&scheds[i]);
        
        // Finalize argobots
        ABT_finalize();

        // Free allocated memory
        free(xstreams);
        free(pools);
        free(scheds);

        free(mailBox);
        free(requestBox);
        free(sharedCounter);
        free(requestSent);
        free(requestServed);

        free(pool_head_push);
        free(pool_head_pop);
        free(pool_tail_push);
        free(pool_tail_pop);
        free(pool_stolen_from);
        free(pool_stole_from);
        free(pool_tasks_created);
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
        return p_pool->p_head ? ABT_FALSE : ABT_TRUE;   // Return true if head is NULL
}

// Pops a thread from the head of the pool
// If the pool is empty (head == NULL), then send a request to a victim worker and wait for a task to be donated
// This function handles both the default pool pushes and pops, along with random work stealing
static ABT_thread pool_pop(ABT_pool pool, ABT_pool_context context)
{
        pool_t *p_pool;
        ABT_pool_get_data(pool, (void **)&p_pool);
        unit_t *p_unit = NULL;

        int rank;
        ABT_xstream_self_rank(&rank);

        int target;                     // Used to send requests
        int requesterRank;              // Used to figure out who sent the request

        // First check if there is a request in the request-box and there is a task in the deque
        requesterRank = requestBox[rank];       // Note: 32bit reads are atomic in x86
        if (requesterRank != -1 && p_pool->p_tail)
        {
                // Pop from the Tail
                p_unit = p_pool->p_tail;
                
                // Move the tail pointer one step closer to the head
                p_pool->p_tail = p_unit->p_next;

                // Update statistics
                pool_tail_pop[rank]++;
                pool_stolen_from[rank]++;

                const int one = 1;
                __sync_fetch_and_sub(&sharedCounter[rank], &one);   // Decrement shared counter due to pop from tail

                // TODO: Move to atomics
                mailBox[requesterRank] = p_unit;        // Put the popped thread on the requesters Mailbox
                requestBox[rank] = -1;   // Clear The request
                requestServed[requesterRank] = true;
        }
        
        // Next send a request if there is no task in the dequeue
        // TODO: Move to atomics
        if (p_pool->p_head == NULL)
        {
                pthread_mutex_lock(&p_pool->lock);
                // First Check the Mailbox for a task
                if (mailBox[rank] != NULL)
                {
                        // printf("MailBox Non-Empty at %d\n", rank);
                        // Take Mutex on Mailbox as it can be Written by the Victim as well
                        // pthread_mutex_lock(&p_pool->lock);
                        // {
                                // There is a task in Mailbox; pop it
                                p_unit = mailBox[rank]; // Variable that returns the thread
                                mailBox[rank] = NULL;   // Empty the Mailbox
                        // }
                        // pthread_mutex_unlock(&p_pool->lock);
                }
                else if(!requestSent[rank])
                {
                        for(int i = (rank + 1) % num_xstreams; i != rank; i = (i + 1) % num_xstreams)
                        {
                                // Both Deque and Mailbox are empty
                                // Send request to a Worker with non-empty deque
                                target = i;

                                pthread_mutex_lock(&pplock);
                                int targetRequestBox = requestBox[target];
                                pthread_mutex_unlock(&pplock);

                                if (sharedCounter[target] >= 10 && targetRequestBox == -1)
                                {
                                        // Take lock on Request Box as it is read by the Victim as well
                                        pthread_mutex_lock(&pplock);
                                        // Put a Steal Request in the Request Box
                                        // printf("Current Request box value seen by Worker %d for Target %d is %d\n", rank, target, requestBox[target]);
                                        requestBox[target] = rank;      //Critical Section as multiple workers may be able to put request
                                        pthread_mutex_unlock(&pplock);

                                        // printf("Request Sent by Worker %d to Worker %d\n", rank, target);
                                        requestSent[rank] = true;
                                        break;
                                }
                        } // TODO: Potential Deadlock if only 2 ES and Stealer is empty and the other worker has only 1 or 0 threads
                } else if(requestServed[rank]){
                        requestSent[rank] = false;
                        requestServed[rank] = false;
                }
                pthread_mutex_unlock(&p_pool->lock);
        }
        else if (p_pool->p_head == p_pool->p_tail) // Finally pop from head if a task exists in the dequeue
        {
                { /* Only one thread. */
                        p_unit = p_pool->p_head;
                        p_pool->p_head = NULL;
                        p_pool->p_tail = NULL;
                        pool_head_pop[rank]++;
                        const int one = 1;
                        __sync_fetch_and_sub(&sharedCounter[rank], &one);   // Decrement shared counter due to pop from head
                }
        }
        else
        {
                /* Pop from the head. */
                p_unit = p_pool->p_head;
                p_pool->p_head = p_unit->p_prev;

                pool_head_pop[rank]++;
                const int one = 1;
                __sync_fetch_and_sub(&sharedCounter[rank], &one);   // Decrement shared counter due to pop from head
        }

        if (!p_unit)
                return ABT_THREAD_NULL;
        
        pool_net_pop[rank]++;
        return p_unit->thread;
}

// Push a task into the pool from the head end. Only the handler of the pool can call this function when fork is called
static void pool_push(ABT_pool pool, ABT_unit unit, ABT_pool_context context)
{
        pool_t *p_pool;
        ABT_pool_get_data(pool, (void **)&p_pool);
        unit_t *p_unit = (unit_t *)unit;
        printf("DBG: %p %p\n", (void *)pool, (void *)p_pool);

        int rank;
        ABT_xstream_self_rank(&rank);

        pool_net_push[rank]++;
        /* Push to the head. */
        if (p_pool->p_head)     // If head is not NULL i.e pool is not empty
        {
                p_unit->p_prev = p_pool->p_head;
                p_pool->p_head->p_next = p_unit;
                p_pool->p_head = p_unit;
        }
        else    // If head is null i.e. dequeue is empty
        {
                p_pool->p_head = p_unit;
                p_pool->p_tail = p_unit;
        }
        pool_head_push[rank]++;
        __sync_fetch_and_add(&sharedCounter[rank], 1);  // Atomically increment the shared counter as multiple workers might be reading it
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
        // Create a custom pool user definition
        ABT_pool_user_def def;
        ABT_pool_user_def_create(pool_create_unit, pool_free_unit, pool_is_empty, pool_pop, pool_push, &def);
        ABT_pool_user_def_set_init(def, pool_init);
        ABT_pool_user_def_set_free(def, pool_free);

        /* Pool configuration */
        ABT_pool_config config;
        ABT_pool_config_create(&config);

        /* The same as a pool created by ABT_pool_create_basic(). */
        const int automatic = 1;
        ABT_pool_config_set(config, ABT_pool_config_automatic.key, ABT_pool_config_automatic.type, &automatic);

        for (int i = 0; i < num; i++)
                ABT_pool_create(def, config, &pools[i]);
        
        ABT_pool_user_def_free(&def);
        ABT_pool_config_free(&config);
}

// Creating Scheduler for Work Stealing
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
        ABT_pool *pools;
        ABT_bool stop;

        pools = (ABT_pool*) malloc(sizeof(ABT_pool));
        ABT_sched_get_data(sched, (void **)&p_data);
        ABT_sched_get_pools(sched, 1, 0, pools);        // Fetch one pool starting from index 0, i.e. the primary pool

        while (1)
        {
                // Execute one work unit from the scheduler's primary pool
                ABT_thread thread;
                ABT_pool_pop_thread_ex(pools[0], &thread, ABT_POOL_CONTEXT_OWNER_PRIMARY);  // Pop a thread from the primary pool as the primary handler
                if (thread != ABT_THREAD_NULL)  // Was able to pop a thread from the primary pool
                        ABT_self_schedule(thread, ABT_POOL_NULL);
                else
                        break;  // Exit if there is no more work left to do

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

        ABT_sched_config_var cv_event_freq = {.idx = 0,
                                              .type = ABT_SCHED_CONFIG_INT};

        ABT_sched_def sched_def = {.type = ABT_SCHED_TYPE_ULT,
                                   .init = sched_init,
                                   .run = sched_run,
                                   .free = sched_free,
                                   .get_migr_pool = NULL};

        ABT_sched_config_create(&config, cv_event_freq, 10, ABT_sched_config_var_end);  // Create a scheduler config

        my_pools = (ABT_pool *)malloc(num * sizeof(ABT_pool));
        for(int i = 0; i < num; i++)
        {
                for(int k = 0; k < num; k++)
                        my_pools[k] = pools[(k + i) % num];     // my_pools[0] is the primary pool
                ABT_sched_create(&sched_def, num, my_pools, config, &scheds[i]);
        }
        free(my_pools);
        ABT_sched_config_free(&config);
}
