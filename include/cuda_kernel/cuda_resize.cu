#include "cuda_resize.cuh"

namespace cuda_kernel
{
    __global__ void testsumArrayKernel(float *C, float *A, float *B)
    {
        int i = threadIdx.x;
        C[i] = A[i] + B[i];
    }
}