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

extern "C"      // Import C style functions 
{
        #include "argolib.h"
}

template<typename T>
void lambda_wrapper(void *arg) {
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
                argolib_init(argc, argv);
        }

        // Finalizes the ArgoLib runtime, and performs the cleanup
        void finalize()
        {
                argolib_finalize();
        }

        // Runs the top level recursive parallel prgram passed as the lambda
        template <typename T>
        void kernel(T &&lambda)
        {
                typedef typename std::remove_reference<T>::type U;
                return argolib_kernel(lambda_wrapper<U>, new U(lambda));
        }

        // Creates a new ULT to run lambda and returns the task handle to the ULT
        template <typename T>
        Task_handle* fork(T &&lambda)
        {
                typedef typename std::remove_reference<T>::type U;
                return argolib_fork(lambda_wrapper<U>, new U(lambda));
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
                
                argolib_join(list, size);       // Pass the list and the size to argolib
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
