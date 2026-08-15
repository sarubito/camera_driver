#ifndef CUDA_RESIZE_CUH
#define CUDA_RESIZE_CUH

#include <cuda_runtime.h>

namespace cuda_kernel
{
    __global__ void testsumArrayKernel(float *C, float *A, float *B);
}

#endif //CUDA_RESIZE_CUH