#include "cuda_kernel.hpp"

namespace cuda_kernel
{
    void launchTestSumArrayKernel(float *C, float *A, float *B, int size)
    {

        // 出力配列CをGPU上に確保
        CUDA_CHECK(cudaMalloc((void**)&C, size * sizeof(float)));

        // 入力配列AとBをGPU上に確保
        CUDA_CHECK(cudaMalloc((void**)&A, size * sizeof(float)));
        CUDA_CHECK(cudaMalloc((void**)&B, size * sizeof(float)));

        // kernel関数のポインタをGPU上に確保
        void* testsumArrayKernel = nullptr;
        CUDA_CHECK(cudaMalloc((void**)&testsumArrayKernel, sizeof(void*)));

        // 入力配列AとBのアドレスを取得し、取得したアドレスにホスト側の配列AとBの内容をコピーする
        void* constant_address = nullptr;
        CUDA_CHECK(cudaGetSymbolAddress(&constant_address, A));
        CUDA_CHECK(cudaMemcpy(constant_address, A, size * sizeof(float), cudaMemcpyHostToDevice));
        constant_address = nullptr;
        CUDA_CHECK(cudaGetSymbolAddress(&constant_address, B));
        CUDA_CHECK(cudaMemcpy(constant_address, B, size * sizeof(float), cudaMemcpyHostToDevice));

        // kernel関数のアドレスを取得し、取得したアドレスにkernel関数のポインタをコピーする
        // 関数ポインタをコピーすることで、GPU側でkernel関数を呼び出すことができるようになる
        // 呼び出す際には、cudaLaunchKernel関数を使用する
        // cudaLaunchKernelだけでもkernel関数を呼び出すことはできるが、関数ポインタをコピーすることで、GPU側でkernel関数を呼び出すことができるようになる
        // これにより、明示的にCUDAで関数を実行していることがわかるようになる
        constant_address = nullptr;
        CUDA_CHECK(cudaGetSymbolAddress(&constant_address, &testsumArrayKernel));
        CUDA_CHECK(cudaMemcpy(constant_address, &testsumArrayKernel, sizeof(void*), cudaMemcpyHostToDevice));

        // CUDA launchでkernel関数を呼び出す
        // GPU側にすでに関数ポインタがコピーされているので、cudaLaunchKernel関数を使用してkernel関数を呼び出すことができる
        void* kernel_args[] = { &C, &A, &B };
        CUDA_CHECK(cudaLaunchKernel(testsumArrayKernel, dim3(size), dim3(1), kernel_args, 0, nullptr));

        // // 別の方法として、<<<>>>構文を使用してkernel関数を呼び出すこともできるが、
        // 関数ポインタをコピーすることで、GPU側でkernel関数を呼び出すことができるようになる
        // どちらもGPUで実行されており、以下の方法のほうがコードの可読性が高い
        // 以降のkernel関数の呼び出しは、<<<>>>構文を使用することにする
        // testsumArrayKernel<<<size, 1>>>(C, A, B); 

        // エラーの確認
        CUDA_CHECK(cudaGetLastError());
        // GPU側での処理が完了するまで待機する
        CUDA_CHECK(cudaDeviceSynchronize());

        float host_C = 0;
        CUDA_CHECK(cudaMemcpy(&host_C, C, size * sizeof(float), cudaMemcpyDeviceToHost));

        // Mallocで確保したメモリを解放する
        CUDA_CHECK(cudaFree(C));
        CUDA_CHECK(cudaFree(A));
        CUDA_CHECK(cudaFree(B));
        CUDA_CHECK(cudaFree(testsumArrayKernel));
    }

    void launchYUV422ToRGBA8_CUDAKernel(const std::vector<std::uint8_t>& yuyv, 
        std::vector<std::uint8_t>& rgba, int width, int height, std::optional<std::string>& error_string)
    {
        int num_blocks = (width * height) / 2;
        size_t yuyv_size = yuyv.size();
        size_t rgba_size = rgba.size();

        //GPU上の保存されているアドレスを保管するための変数
        std::uint8_t *d_yuyv, *d_rgba;

        // GPUメモリ(VRAM)の確保
        cudaMalloc((void **)&d_yuyv, yuyv_size);
        cudaMalloc((void **)&d_rgba, rgba_size);

        // ホスト(CPU)からデバイス(GPU)へ入力データを転送
        cudaMemcpy(d_yuyv, yuyv.data(), yuyv_size, cudaMemcpyHostToDevice);

        // スレッド配置の設定(1ブロックあたり256スレッド)
        int threadsPerBlock = 256;
        int blocksPerGrid = (num_blocks + threadsPerBlock - 1) / threadsPerBlock;

        // CUDAカーネル実行
        yuv422_to_rgba8_gpu<<<blocksPerGrid, threadsPerBlock>>>(d_yuyv, d_rgba, num_blocks);

        // 同期とエラーチェック
        cudaDeviceSynchronize();
        cudaError_t err = cudaGetLastError();
        if(err != cudaSuccess){
            error_string = std::string("CUDA Error: ") + cudaGetErrorString(err);
        }

        // デバイス(GPU)からホスト(CPU)へ結果を転送
        cudaMemcpy(rgba.data(), d_rgba, rgba_size, cudaMemcpyDeviceToHost);

        // GPUメモリの解放
        cudaFree(d_yuyv);
        cudaFree(d_rgba);
    }

    void launchYUV422ToRGBA8_CUDAKernel(const std::uint8_t * yuyv, size_t yuyv_size,
        std::vector<std::uint8_t>& rgba, int width, int height, std::optional<std::string>& error_string)
    {
        int num_blocks = (width * height) / 2;
        size_t rgba_size = rgba.size();

        std::uint8_t *d_yuyv, *d_rgba;

        // GPUメモリ(VRAM)の確保
        cudaMalloc((void **)&d_yuyv, yuyv_size);
        cudaMalloc((void **)&d_rgba, rgba_size);

        // ホスト(CPU)からデバイス(GPU)へ入力データを転送（ホストポインタ版）
        cudaMemcpy(d_yuyv, yuyv, yuyv_size, cudaMemcpyHostToDevice);

        int threadsPerBlock = 256;
        int blocksPerGrid = (num_blocks + threadsPerBlock - 1) / threadsPerBlock;

        yuv422_to_rgba8_gpu<<<blocksPerGrid, threadsPerBlock>>>(d_yuyv, d_rgba, num_blocks);

        cudaDeviceSynchronize();
        cudaError_t err = cudaGetLastError();
        if(err != cudaSuccess){
            error_string = std::string("CUDA Error: ") + cudaGetErrorString(err);
        }

        cudaMemcpy(rgba.data(), d_rgba, rgba_size, cudaMemcpyDeviceToHost);

        cudaFree(d_yuyv);
        cudaFree(d_rgba);
    }

    bool checkCUDA()
    {
        bool ret = false;
        int device_count = 0;
        cudaError_t err = cudaGetDeviceCount(&device_count);
        ret = (err == cudaSuccess && device_count > 0);
        return ret; 
    }
}