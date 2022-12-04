#include <stdio.h>
#include <stdlib.h>
#include <argolib.h>

typedef struct {
    int n;
    int ret;
} fibonacci_arg_t;


void fib(fibonacci_arg_t* fib_arg)
{
    if (fib_arg->n < 2){
        fib_arg->ret = fib_arg->n;
        return;
    }

    fibonacci_arg_t *x, *y;
    x = (fibonacci_arg_t*) malloc(sizeof(fibonacci_arg_t));
    y = (fibonacci_arg_t*) malloc(sizeof(fibonacci_arg_t));
    x->n = fib_arg->n - 1;
    y->n = fib_arg->n - 2;

    Task_handle* task1 = argolib_fork((fork_t)fib, (void*)x);
    Task_handle* task2 = argolib_fork((fork_t)fib, (void*)y);
    Task_handle* task_pair[2] = {task1, task2};
    argolib_join(task_pair, 2);

    fib_arg->ret = x->ret + y->ret;

    free(x);
    free(y);
}

void compute_kernel(fibonacci_arg_t *arg, int *result)
{
    argolib_core_kernel((fork_t)fib, (void*)arg);
    *result += arg->ret;
}

int main(int argc, char **argv) {
    argolib_init(argc, argv);
    int result = 0;
    fibonacci_arg_t* arg;
    arg = (fibonacci_arg_t*) malloc(sizeof(fibonacci_arg_t));
    arg->n = 15;
    arg->ret = 0;
    for(int i = 0; i < 10; i++)
    {
        argolib_start_tracing();
        compute_kernel(arg, &result);
        argolib_stop_tracing();
    }
    printf("10 * Fib(15) = %d\n", result);
    argolib_finalize();
    free(arg);
    return 0;
}
