#include "cuda_kernel.hpp"

// CUDA最適化メモ:
// - GPUのYUV入力バッファとRGBA出力バッファは毎フレームごとにmalloc/freeせず、
//   一度確保した永続バッファを再利用する。
// - Host -> Device / Device -> Host の転送は cudaMemcpyAsync を使用し、
//   CPUが結果を必要とする直前で一度だけ cudaStreamSynchronize する。
// - これにより、毎フレームのメモリ確保オーバーヘッドと同期コストを抑える。
// - ただし、結果をCPUで使う前には必ず同期が必要なので、終端でまとめて待つ構成にしている。

namespace cuda_kernel
{
    namespace
    {
        std::uint8_t* g_d_yuyv = nullptr;
        std::uint8_t* g_d_rgba = nullptr;
        size_t g_yuyv_capacity = 0;
        size_t g_rgba_capacity = 0;
        cudaStream_t g_stream = nullptr;

        void ensureCUDAStream()
        {
            if(g_stream == nullptr)
            {
                CUDA_CHECK(cudaStreamCreateWithFlags(&g_stream, cudaStreamNonBlocking));
            }
        }

        void ensureCUDAImageBuffers(size_t yuyv_size, size_t rgba_size)
        {
            ensureCUDAStream();

            if(g_d_yuyv == nullptr || g_yuyv_capacity < yuyv_size)
            {
                if(g_d_yuyv != nullptr)
                {
                    cudaFree(g_d_yuyv);
                }
                CUDA_CHECK(cudaMalloc(reinterpret_cast<void **>(&g_d_yuyv), yuyv_size));
                g_yuyv_capacity = yuyv_size;
            }

            if(g_d_rgba == nullptr || g_rgba_capacity < rgba_size)
            {
                if(g_d_rgba != nullptr)
                {
                    cudaFree(g_d_rgba);
                }
                CUDA_CHECK(cudaMalloc(reinterpret_cast<void **>(&g_d_rgba), rgba_size));
                g_rgba_capacity = rgba_size;
            }
        }
    }

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

        ensureCUDAImageBuffers(yuyv_size, rgba_size);

        // ホスト(CPU)からデバイス(GPU)へ入力データを転送
        CUDA_CHECK(cudaMemcpyAsync(g_d_yuyv, yuyv.data(), yuyv_size, cudaMemcpyHostToDevice, g_stream));

        // スレッド配置の設定(1ブロックあたり512スレッド)
        int threadsPerBlock = 512;
        int blocksPerGrid = (num_blocks + threadsPerBlock - 1) / threadsPerBlock;

        // CUDAカーネル実行
        yuv422_to_rgba8_gpu<<<blocksPerGrid, threadsPerBlock, 0, g_stream>>>(g_d_yuyv, g_d_rgba, num_blocks);

        // エラーチェック（同期はまだ行わない）
        cudaError_t err = cudaGetLastError();
        if(err != cudaSuccess){
            error_string = std::string("CUDA Error: ") + cudaGetErrorString(err);
            return;
        }

        // デバイス(GPU)からホスト(CPU)へ結果を転送
        CUDA_CHECK(cudaMemcpyAsync(rgba.data(), g_d_rgba, rgba_size, cudaMemcpyDeviceToHost, g_stream));

        // CPUが結果を参照する直前でまとめて同期
        CUDA_CHECK(cudaStreamSynchronize(g_stream));
        err = cudaGetLastError();
        if(err != cudaSuccess){
            error_string = std::string("CUDA Error: ") + cudaGetErrorString(err);
        }
    }

    void launchYUV422ToRGBA8_CUDAKernel(const std::uint8_t * yuyv, size_t yuyv_size,
        std::vector<std::uint8_t>& rgba, int width, int height, std::optional<std::string>& error_string)
    {
        int num_blocks = (width * height) / 2;
        size_t rgba_size = rgba.size();

        ensureCUDAImageBuffers(yuyv_size, rgba_size);

        // ホスト(CPU)からデバイス(GPU)へ入力データを転送（ホストポインタ版）
        CUDA_CHECK(cudaMemcpyAsync(g_d_yuyv, yuyv, yuyv_size, cudaMemcpyHostToDevice, g_stream));

        // 1ブロックあたりのスレッド数
        int threadsPerBlock = 256;
        // 1グリッドあたりのブロック数
        int blocksPerGrid = (num_blocks + threadsPerBlock - 1) / threadsPerBlock;

        // デフォルトの解像度に対する値として
        // blocksPerGrid = 1471
        // threadPerBlock = 256
        // 1グリッドあたり1471個のブロックで、1ブロックあたり256個のスレッドで実行
        yuv422_to_rgba8_gpu<<<blocksPerGrid, threadsPerBlock, 0, g_stream>>>(g_d_yuyv, g_d_rgba, num_blocks);

        cudaError_t err = cudaGetLastError();
        if(err != cudaSuccess){
            error_string = std::string("CUDA Error: ") + cudaGetErrorString(err);
            return;
        }

        CUDA_CHECK(cudaMemcpyAsync(rgba.data(), g_d_rgba, rgba_size, cudaMemcpyDeviceToHost, g_stream));
        CUDA_CHECK(cudaStreamSynchronize(g_stream));
        err = cudaGetLastError();
        if(err != cudaSuccess){
            error_string = std::string("CUDA Error: ") + cudaGetErrorString(err);
        }
    }

    bool checkCUDA()
    {
        bool ret = false;
        int device_count = 0;
        cudaError_t err = cudaGetDeviceCount(&device_count);
        ret = (err == cudaSuccess && device_count > 0);
        return ret; 
    }

    void freeCUDA()
    {
        if(g_stream != nullptr)
        {
            CUDA_CHECK(cudaStreamSynchronize(g_stream));
            CUDA_CHECK(cudaStreamDestroy(g_stream));
            g_stream = nullptr;
        }

        if(g_d_yuyv != nullptr)
        {
            CUDA_CHECK(cudaFree(g_d_yuyv));
            g_d_yuyv = nullptr;
            g_yuyv_capacity = 0;
        }

        if(g_d_rgba != nullptr)
        {
            CUDA_CHECK(cudaFree(g_d_rgba));
            g_d_rgba = nullptr;
            g_rgba_capacity = 0;
        }
    }
}