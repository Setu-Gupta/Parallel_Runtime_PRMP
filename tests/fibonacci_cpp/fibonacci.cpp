#include <iostream>
#include <argolib.hpp>
#include <mutex>

std::mutex m;

int fib(int n)
{
        if(n < 2)
                return n;
        int x, y;
        Task_handle* task1 = argolib::fork([&]() { x = fib(n-1);
                        m.lock();
                        std::cout << "for " << n-1 << " x:" << x << std::endl;
                        m.unlock();
                        });
        Task_handle* task2 = argolib::fork([&]() { y = fib(n-2);
                        m.lock();
                        std::cout << "for " << n-2 << " y:" << y << std::endl;
                        m.unlock();
                        });
        argolib::join(task1, task2);
        m.lock();
        std::cout << "n:" << n << " x:" << x << " y:" << y << std::endl;
        m.unlock();
        return x + y;
}
int main(int argc, char **argv)
{
        argolib::init(argc, argv);
        int result;
        argolib::kernel([&]() {
                        result = fib(4);
                        m.lock();
                        std::cout << "result: " << result << std::endl;
                        m.unlock();
                        });
        std::cout << "Fib(4) = " << result << std::endl;
        argolib::finalize();
        return 0;
}
