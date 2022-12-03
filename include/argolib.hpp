// Ref: https://caiorss.github.io/C-Cpp-Notes/passing-lambda.html
// Ref: https://en.cppreference.com/w/cpp/utility/initializer_list
// Ref: https://stackoverflow.com/questions/11030517/unknown-number-of-arguments-in-function
// Ref: https://caiorss.github.io/C-Cpp-Notes/passing-lambda.html
// Ref: https://stackoverflow.com/questions/73976001/g-undefined-reference-to-while-linking-with-a-custom-shared-library/73977954#73977954
// Ref: https://github.com/vivkumar/cse502/blob/master/hclib/inc/hclib_cpp.h

#ifndef __ARGOLIB_HPP__
#define __ARGOLIB_HPP__

#include <initializer_list>
#include <functional>
#include <type_traits>
#include <pthread.h>
#include "pcm.h"
#include <chrono>
#include <thread>

extern "C"      // Import C style functions 
{
        #include "./../src/include/argolib_core.h"
}

pthread_t profiler;     // Pthread for the profiling daemon
bool configureCalled = false;
// Define the function to handle degree of prallelism
void configure_DOP(double jpi_prev, double jpi_cur)
{
        // std::cout << "JPIs: Prev: " << jpi_prev << " Cur: " << jpi_cur << std::endl;
        int INC = 1;
        int DEC = -1;
        static int lastAction = INC;
        const int wChange = 1; // find experimentally on your system

        if(!configureCalled)
        {
                configureCalled = true;
                xstream_lullaby(wChange);
                lastAction = DEC;
                return;
        }
        if (jpi_cur < jpi_prev)
        {
                if (lastAction == DEC)
                {
                        xstream_lullaby(wChange);
                }
                else
                {
                        xstream_alarm(wChange);
                }
        }
        else
        {
                if (lastAction == DEC)
                {
                        xstream_alarm(wChange);
                        lastAction = INC;
                }
                else
                {
                        xstream_lullaby(wChange);
                        lastAction = DEC;
                }
        }
}

// Create the daemon worker function
// Ref: https://stackoverflow.com/questions/4184468/sleep-for-milliseconds
bool daemon_shutdown = false;
void* daemon_profiler(void *)
{
        // Update DOP every 100ms
        const unsigned int fixed_interval = 100;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));   // Sleep for 5 milliseconds to wait for warmup
        
        double jpi_prev = 0;
        while(!daemon_shutdown)
        {
                double jpi_cur = logger::get_jpi();
                std::cout << jpi_cur << std::endl;
                configure_DOP(jpi_prev, jpi_cur);
                jpi_prev = jpi_cur;
                std::this_thread::sleep_for(std::chrono::microseconds(fixed_interval));
        }
        return NULL;
}

template<typename T>
void lambda_wrapper(void *arg)
{
    T* lambda = static_cast<T*>(arg);
    (*lambda)();
    delete lambda;
}

namespace argolib
{
         // Initializes the ArgoLib runtime.
         // It should be the first thing to call in the user main.
         // Arguments “argc” and “argv” are the ones passed in the call to user main method.
        void init(int argc, char **argv)
        {
                logger::start();

                // Set up the profiler
                if(pthread_create(&profiler, NULL, &daemon_profiler, NULL) != 0)
                {
                        printf("[ERR]: Could not create profiler!\n");
                        exit(-1);
                }

                argolib_core_init(argc, argv);
        }

        // Finalizes the ArgoLib runtime, and performs the cleanup
        void finalize()
        {
                // Stop the daemon
                daemon_shutdown = true;

                argolib_core_finalize();
        }

        // Runs the top level recursive parallel prgram passed as the lambda
        template <typename T>
        void kernel(T &&lambda)
        {
                typedef typename std::remove_reference<T>::type U;
                return argolib_core_kernel(lambda_wrapper<U>, new U(lambda));
        }

        // Creates a new ULT to run lambda and returns the task handle to the ULT
        template <typename T>
        Task_handle* fork(T &&lambda)
        {
                typedef typename std::remove_reference<T>::type U;
                return argolib_core_fork(lambda_wrapper<U>, new U(lambda));
        }

        // Called by join to join multiple tasks
        // Takes the input as a initializer list of task handles
        // Finally calls argolib_join on the list of handles
        void join_impl(std::initializer_list<Task_handle*> handles)
        {
                int size = handles.size();      // Get the number of handles passed
                Task_handle** list = (Task_handle**)malloc(size * sizeof(Task_handle*));        // Create an array of handles to be passed to argolib
                
                // Populate the list by iterating over handles
                int i = 0;
                for(Task_handle* th : handles)
                        list[i++] = th;
                
                argolib_core_join(list, size);       // Pass the list and the size to argolib
                free(list);
        }
        
        // Called to join multiple tasks via their task handles
        // This function takes variable (unknown) number of arguments
        template<typename... T>
        void join(T ...handles)
        {
                join_impl({handles...});    // Pass on all the arguments to the join_impl as a initializer list
        }
}

#endif
