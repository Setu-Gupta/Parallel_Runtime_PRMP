#include <cstring>
#include "timer.h"
#include <pthread.h>

// Ref: https://www.geeksforgeeks.org/thread-functions-in-c-c/
/*
 * Ported from HJlib
 *
 * Author: Vivek Kumar
 *
 */
//48 * 256 * 2048
#define SIZE 10485760
#define ITERATIONS 65536
#define THRESHOLD 2048

double* myNew, *myVal;
int n;

int ceilDiv(int d) {
        int m = SIZE / d;
        if (m * d == SIZE) {
                return m;
        } else {
                return (m + 1);
        }
}

typedef struct
{
        uint64_t low;
        uint64_t high;
} args_t;

void* pthread_recurse(void *args)
{
        uint64_t low = ((args_t*)args)->low;
        uint64_t high = ((args_t*)args)->high;
        if((high - low) > THRESHOLD) {
                uint64_t mid = (high+low)/2;
                
                pthread_t child;
                
                args_t child_args;
                child_args.low = low;
                child_args.high = mid;

                args_t child2_args;
                child2_args.low = mid;
                child2_args.high = high;
                
                pthread_create(&child, NULL, &pthread_recurse, (void*)&child_args);
                pthread_recurse((void*)&child2_args);
                pthread_join(child, NULL);
        } else {
                for(uint64_t j=low; j<high; j++) {
                        myNew[j] = (myVal[j - 1] + myVal[j + 1]) / 2.0;
                }
        }
        return NULL;
}

void recurse(uint64_t low, uint64_t high) {
        args_t args;
        args.low = low;
        args.high = high;
        pthread_recurse(&args);
}

void runParallel() {
        for(int i=0; i<ITERATIONS; i++) {
                recurse(1, SIZE+1);
                double* temp = myNew;
                myNew = myVal;
                myVal = temp;
        }
}

int main() {
        myNew = new double[(SIZE + 2)];
        myVal = new double[(SIZE + 2)];
        memset(myNew, 0, sizeof(double) * (SIZE + 2));
        memset(myVal, 0, sizeof(double) * (SIZE + 2));
        myVal[SIZE + 1] = 1.0;
        timer::kernel("Iterative Averaging Kernel", [=]() {
                        runParallel();
                        });
        delete(myNew);
        delete(myVal);  
}
