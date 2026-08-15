#include "cuda_yuv422_to_rgba8.cuh"

namespace cuda_kernel
{
    __global__ void yuv422_to_rgba8_gpu(std::uint8_t *yuyv, std::uint8_t *rgba, int num_blocks)
    {
        int i = blockIdx.x * blockDim.x + threadIdx.x;

        if(i >= num_blocks) return;

        int yuv_idx = i * 4;
        int rgba_idx = i * 8;

        int y0 = *(yuyv + yuv_idx + 0);
        int u = *(yuyv + yuv_idx + 1) - 128;
        int y1 = *(yuyv + yuv_idx + 2);
        int v = *(yuyv + yuv_idx + 3) - 128;

        // BT.601規格に基づく整数演算
        int r_coeff = (11585 * v) >> 13;
        int g_coeff = ((-2901 * u) - (5902 * v)) >> 13;
        int b_coeff = (14624 * u) >> 13;

        // 一時的な計算結果の格納用
        int r, g, b;

        // 1番目のピクセルを処理
        r = y0 + r_coeff;
        g = y0 + g_coeff;
        b = y0 + b_coeff;

        // 0~255に丸める
        *(rgba + rgba_idx + 0) = (r < 0) ? 0 : ((r > 255) ? 255 : static_cast<uint8_t>(r)); // R
        *(rgba + rgba_idx + 1) = (g < 0) ? 0 : ((g > 255) ? 255 : static_cast<uint8_t>(g)); // G
        *(rgba + rgba_idx + 2) = (b < 0) ? 0 : ((b > 255) ? 255 : static_cast<uint8_t>(b)); // B
        *(rgba + rgba_idx + 3) = 255;                                                       // A

        // 2番目のピクセルを処理
        r = y1 + r_coeff;
        g = y1 + g_coeff;
        b = y1 + b_coeff;

        // 0~255に丸める
        *(rgba + rgba_idx + 4) = (r < 0) ? 0 : ((r > 255) ? 255 : static_cast<uint8_t>(r)); // R
        *(rgba + rgba_idx + 5) = (g < 0) ? 0 : ((g > 255) ? 255 : static_cast<uint8_t>(g)); // G
        *(rgba + rgba_idx + 6) = (b < 0) ? 0 : ((b > 255) ? 255 : static_cast<uint8_t>(b)); // B
        *(rgba + rgba_idx + 7) = 255; 

    }
}