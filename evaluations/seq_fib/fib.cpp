#include "timer.h"

#define FIB_N 45
#define THRESHOLD 25

int fib_seq(int n) {
    if(n<2) return n;
    else return fib_seq(n-1) + fib_seq(n-2);
}

int fib(int n) {
    if(n <= THRESHOLD) {
        return fib_seq(n);
    } else {
	int x, y;
        x = fib(n-1);
	y = fib(n-2);
	return x + y;
    }
}

int main() { 
    int res;
    timer::kernel("Fib kernel", [&]() {
        res = fib(FIB_N);
    });
    printf("Fib[%d]: %d\n", FIB_N, res);
}
