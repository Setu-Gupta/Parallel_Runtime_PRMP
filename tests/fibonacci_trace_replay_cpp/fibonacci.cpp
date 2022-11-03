#include <iostream>
#include <argolib.hpp>

int fib(int n)
{
	if(n < 2)
                return n;
        int x = 0, y = 0;
        Task_handle* task1 = argolib::fork([&]() { x = fib(n-1);});
        Task_handle* task2 = argolib::fork([&]() { y = fib(n-2);});
        argolib::join(task1, task2);
	return x + y;
}

void compute_kernel(int& result)
{
        result += fib(20);
}

int main(int argc, char **argv)
{
        argolib::init(argc, argv);
        int result = 0;
        for(int i = 0; i < 10; i++)
        {
                argolib::start_tracing();
                compute_kernel(result);
                argolib::stop_tracing();
        }
        std::cout << "10 * Fib(20) = " << result << std::endl;
        argolib::finalize();
        return 0;
}
