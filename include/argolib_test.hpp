// Ref: https://caiorss.github.io/C-Cpp-Notes/passing-lambda.html
// Ref: https://en.cppreference.com/w/cpp/utility/initializer_list
// Ref: https://stackoverflow.com/questions/11030517/unknown-number-of-arguments-in-function
// Ref: https://caiorss.github.io/C-Cpp-Notes/passing-lambda.html

#ifndef __ARGOLIB_HPP__
#define __ARGOLIB_HPP__

#include <initializer_list>
#include <argolib.h>
#include <functional>
#include <iostream>

typedef ABT_thread Task_handle;

typedef void (*test_t)(void);
void run_lambda(test_t func)
{
        func();
}

using FunctionCallback = std::function<void(void)>;
namespace CLambdaWorkaround
{

        FunctionCallback& get_callback()
        {   
		static FunctionCallback callback;
                return callback;
        };  

        void set_callback(FunctionCallback func)
        {   
                FunctionCallback& callback = get_callback();
                callback = func;
        }   

        void lambda_adapter()
        {   
                get_callback()();
        }   

        void lambda_wrapper(FunctionCallback func)
        {   
                set_callback(func);
                run_lambda(&lambda_adapter);
        }   

}

namespace argolib
{
         // Initializes the ArgoLib runtime.
         // It should be the first thing to call in the user main.
         // Arguments “argc” and “argv” are the ones passed in the call to user main method.
        void init(int argc, char **argv)
        {
                std::cout << "Called init" << std::endl;
//                argolib_init(argc, argv);
        }

        // Finalizes the ArgoLib runtime, and performs the cleanup
        void finalize()
        {
                std::cout << "Called finalize" << std::endl;
//                argolib_finalize();
        }

        // Runs the top level recursive parallel prgram passed as the lambda
        template <typename T>
        void kernel(T &&lambda)
        {
                std::cout << "Called kernel" << std::endl;
                CLambdaWorkaround::lambda_wrapper(lambda);			
        }

        // Creates a new ULT to run lambda and returns the task handle to the ULT
        template <typename T>
        Task_handle* fork(T &&lambda)
        {
                std::cout << "Called fork" << std::endl;
                CLambdaWorkaround::lambda_wrapper(lambda);			
        }

        // Called by join to join multiple tasks
        // Takes the input as a initializer list of task handles
        // Finally calls argolib_join on the list of handles
        void join_impl(std::initializer_list<Task_handle*> handles)
        {
                std::cout << "Called join_impl" << std::endl;
                
                int size = handles.size();      // Get the number of handles passed
                Task_handle* list[size];        // Create an array of handles to be passed to argolib
                
                // Populate the list by iterating over handles
                int i = 0;
                for(Task_handle* th : handles)
                        list[i++] = th;
                
                // argolib_join(list, size);       // Pass the list and the size to argolib
        }
        
        // Called to join multiple tasks via their task handles
        // This function takes variable (unknown) number of arguments
        template<typename... T>
        void join(T ...handles)
        {
                std::cout << "Called join..." << std::endl;
                join_impl({handles...});    // Pass on all the arguments to the join_impl as a initializer list
        }
}

#endif
