#ifndef CUDA_KERNEL_HPP
#define CUDA_KERNEL_HPP

#include <cstdint>
#include <iostream>
#include <vector>
#include <optional>

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
#include "cuda_yuv422_to_rgba8.cuh"

namespace cuda_kernel
{
    void launchTestSumArrayKernel(float *C, float *A, float *B, int size);
    void launchYUV422ToRGBA8_CUDAKernel(const std::vector<std::uint8_t>& yuyv, 
        std::vector<std::uint8_t>& rgba, int width, int height, std::optional<std::string>& error_string);
    // pointer overload: accept host pointer without allocating a temporary vector
    void launchYUV422ToRGBA8_CUDAKernel(const std::uint8_t * yuyv, size_t yuyv_size,
        std::vector<std::uint8_t>& rgba, int width, int height, std::optional<std::string>& error_string);
    bool checkCUDA();
    void freeCUDA();
}

#endif // CUDA_KERNEL_HPP