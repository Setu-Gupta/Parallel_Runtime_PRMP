#include <iostream>
#include <argolib.hpp>

int fib(int n) {
        if(n<2) return n;
        int x, y;
        Task_handle* task1 = argolib::fork([&]() { x = fib(n-1); });
        Task_handle* task2 = argolib::fork([&]() { y = fib(n-2); });
        argolib::join(task1, task2);
        return x+y;
}
int main(int argc, char **argv) {
        argolib::init(argc, argv);
        int result;
        argolib::kernel([&]() {
                        result = fib(40);
                        });
        std::cout << "Fib(40) = "<<result<<std::endl;
        argolib::finalize();
        return 0;
}
