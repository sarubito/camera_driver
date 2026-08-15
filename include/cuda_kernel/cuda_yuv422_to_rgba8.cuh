#ifndef CUDA_YUV422_TO_RGBA8_CUH
#define CUDA_YUV422_TO_RGBA8_CUH

#include <cstdint>
#include <vector>
#include <iostream>

#include <cuda_runtime.h>

namespace cuda_kernel
{
    __global__ void yuv422_to_rgba8_gpu(std::uint8_t *yuyv, std::uint8_t *rgba, int num_blocks);
}

#endif //CUDA_YUV422_TO_RGBA8_CUH