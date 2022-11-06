#include <cstdlib>
#include <iostream>
#include <argolib.hpp>

#define ITERS   10
#define SIZE    10
#define MAX_VAL 100

#define THRESHOLD       5
void iter_avg(int* arr, int size)
{
        if(size < THRESHOLD)
        {
                for(int i = 0; i < size; i++)
                {
                        int cur =  arr[i];
                        int prev = i-1 >= 0 ? arr[i-1] : 0;
                        int next = i+1 < size ? arr[i+1] : 0;
                        arr[i] = (prev + cur + next)/3;
                }
        }
        else
        {
                int mid = size/2;
                Task_handle* task = argolib::fork([=]()
                                                {
                                                        iter_avg(arr, mid);
                                                });
                iter_avg(arr+mid, size-mid);
                argolib::join(task);
        }
}

int main(int argc, char **argv)
{
        // Create an array of size SIZE and populate it randomly
        int *arr = (int *)calloc(SIZE, sizeof(int));
        for(int i = 0; i < SIZE; i++)
                arr[i] = rand() % MAX_VAL;

        // Print out the array
        std::cout << "Before averaging:" << std::endl;
        for(int i = 0; i < SIZE; i++)
                std::cout << arr[i] << " ";
        std::cout << std::endl; 

        // Run parallel interative averaging
        argolib::init(argc, argv);
        for(int i = 0; i < ITERS; i++)
        {
                argolib::start_tracing();
                argolib::kernel([=](){iter_avg(arr, SIZE);});
                argolib::stop_tracing();
        }
        argolib::finalize();
        
        // Print out the array after averaging
        std::cout << "After averaging:" << std::endl;
        for(int i = 0; i < SIZE; i++)
                std::cout << arr[i] << " ";
        std::cout << std::endl; 

        // Free the array
        free(arr);

        return 0;
}
