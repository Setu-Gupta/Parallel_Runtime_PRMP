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


int main(int argc, char **argv) {
    argolib_init(argc, argv);
    int result;
    fibonacci_arg_t* arg;
    arg = (fibonacci_arg_t*) malloc(sizeof(fibonacci_arg_t));
    arg->n = 27;
    arg->ret = 0;
    for(int i = 0; i < 10; i++){
        argolib_start_tracing();
        argolib_kernel((fork_t)fib, (void*)arg);
        argolib_stop_tracing();
    }
    result = arg->ret;
    printf("Fib(27) = %d\n", result);
    argolib_finalize();
    free(arg);
    return 0;
}
