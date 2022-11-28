#include <iostream>
#include <stdio.h>
#include <time.h>


#define SIZE 10485760
#define ITERATIONS 65536

__global__
void average(double *invec, double *outvec)
{
        int idx = threadIdx.x + blockDim.x * blockIdx.x;
        if(idx <= 0 || idx >= SIZE + 1)
                return;
        outvec[idx] = (invec[idx-1] + invec[idx+1]) / 2.0;
}

int main()
{
        // Crate host and device pointers for input and output arrays
        double *invec, *outvec;
        double *d_invec, *d_outvec;

        // Allocate and initialize arrays on host side
        invec = (double*)malloc((SIZE + 2) * sizeof(double));
        outvec = (double*)malloc((SIZE + 2) * sizeof(double));
        memset(invec, 0, sizeof(double) * (SIZE + 2));
        memset(outvec, 0, sizeof(double) * (SIZE + 2));
        invec[SIZE + 1] = 1.0;

        // Allocate and initialize memory on device side
        cudaMalloc((void**)&d_invec, (SIZE + 2) * sizeof(double));
        cudaMalloc((void**)&d_outvec, (SIZE + 2) * sizeof(double));
        cudaMemset(d_outvec, 0, (SIZE + 2) * sizeof(double));
        
        // Copy the data to the device
        cudaMemcpy(d_invec, invec, (SIZE + 2) * sizeof(double), cudaMemcpyHostToDevice);

        // Create CUDA events to time the kernel execution
        cudaEvent_t start, stop;
        cudaEventCreate(&start);
        cudaEventCreate(&stop);

        // Run iterative averaging
        cudaEventRecord(start);
        for(int i = 0; i < ITERATIONS; i++)
        {
                if(i%2) // Odd iterations
                        average<<<8192, (SIZE + 2)/8192>>>(d_outvec, d_invec);
                else    // Even iterations
                        average<<<8192, (SIZE + 2)/8192>>>(d_invec, d_outvec);
        }
        cudaEventRecord(stop);

        // Copy the data back to host
        cudaMemcpy(outvec, d_outvec, (SIZE + 2) * sizeof(float), cudaMemcpyDeviceToHost);
        
        // Compute the time taken for execution
        cudaEventSynchronize(stop);
        float milliseconds = 0;
        cudaEventElapsedTime(&milliseconds, start, stop);
        std::cout  << "Time taken : " << milliseconds << "ms" << std::endl;

        // Free up memory on device side
        cudaFree(d_invec);
        cudaFree(d_outvec);
        
        // Free up memory on host side
        delete(invec);
        delete(outvec);
}
