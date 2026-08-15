#ifndef CAMERA_DRIVER
#define CAMERA_DRIVER

#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <chrono>
#include <optional>
#include <functional>

#include <fcntl.h>
#include <sys/mman.h>

#include <poll.h>
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>

#include "rclcpp/rclcpp.hpp"

#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "sensor_msgs/image_encodings.hpp"

#include "camera_driver/common/common.hpp"
#include "cuda_kernel/cuda_kernel.hpp"

using namespace cuda_kernel;

namespace camera_driver
{
    class CameraDriver : public rclcpp::Node
    {
        public:
            // 暗黙的型変換が起きないよう、引数が1変数のコンストラクタには必ずexplicit付与するルールになっている。(Google Coding Style)
            explicit CameraDriver(rclcpp::NodeOptions const & options);
            virtual ~CameraDriver();

        private:
            struct Buffer {
                unsigned index;
                void* start;
                size_t length;
            } *buffers_;

            bool onInitialize();
            bool onDeviceInitialize();
            bool onFormatInitialize();
            bool onRequestBufferInitialize();
            bool onMemoryMapInitialize();
            bool onStreamingInitialize();
            bool onParameterInitialize();
            bool onPublisherInitialize();
            bool onOpenCVInitialize();
            bool capture();
            bool setImageToROSMessage(sensor_msgs::msg::Image::SharedPtr image_msg);
            bool publishImageMessage(sensor_msgs::msg::Image::SharedPtr image_msg);
            bool stop();
            void onFastUpdate();
            void onSlowUpdate();
            // pointer versions: avoid copying mmap'd buffer to a temporary vector
            void yuyv422_to_rgba8(const std::uint8_t * yuyv, size_t yuyv_size, 
                std::vector<std::uint8_t>& rgba);
            void yuyv422_to_rgba8_cpu(const std::uint8_t * yuyv, std::vector<std::uint8_t>& rgba);

            rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
            rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr info_pub_;
            rclcpp::TimerBase::SharedPtr image_timer_;
            rclcpp::TimerBase::SharedPtr info_timer_;
            rclcpp::Logger logger_;

            int image_publish_frequency_;
            int info_publish_frequency_;
            int image_freqtotime_;
            int info_freqtotime_;

            std::string camera_frame_id_;
            std::string output_encoding_;
            int original_height_;
            int original_width_;

            std::string device_file_;

            struct v4l2_format fmt_;
            struct v4l2_requestbuffers req_;

            std::uint32_t n_buffers_;

            int fd_;

            bool use_sim_time_;
            bool use_gpu_;
            bool use_opencv_;
            int counter_;

            bool success_flg_;

    };
} // namespace camera_driver

#endif // CAMERA_DRIVER