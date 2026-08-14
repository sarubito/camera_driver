#ifndef CUDA_KERNEL_HPP
#define CUDA_KERNEL_HPP

#include <iostream>

// CUDA_CHECKマクロを定義する
// CUDAを使用する際に、以下のようなマクロを定義して、チェックを行うのが定石
// https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/intro-to-cuda-cpp.html#error-checking-in-cuda
#define CUDA_CHECK(expr_to_check) do {            \
    cudaError_t result  = expr_to_check;          \
    if(result != cudaSuccess)                     \
    {                                             \
        fprintf(stderr,                           \
                "CUDA Runtime Error: %s:%i:%d = %s\n", \
                __FILE__,                         \
                __LINE__,                         \
                result,\
                cudaGetErrorString(result));      \
    }                                             \
} while(0)

#include "cuda_resize.cuh"

namespace cuda_kernel
{
    void launchTestSumArrayKernel(float *C, float *A, float *B, int size);
}

#endif // CUDA_KERNEL_HPP