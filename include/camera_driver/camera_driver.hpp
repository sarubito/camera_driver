#ifndef CAMERA_DRIVER
#define CAMERA_DRIVER

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <chrono>
#include <optional>

#include <linux/videodev2.h>

#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
#include "rclcpp/rclcpp.hpp"

using namespace std::chrono_literals;

namespace camera_driver
{
    class CameraDriver : public rclcpp::Node
    {
        public:
            // 暗黙的型変換が起きないよう、引数が1変数のコンストラクタには必ずexplicit付与するルールになっている。(Google Coding Style)
            explicit CameraDriver(rclcpp::NodeOptions const & options);
            virtual ~CameraDriver();

        private:
            bool onInitialize();
            bool onDeviceInitialize();
            bool onParameterInitialize();
            bool onPublisherInitialize();
            bool onOpenCVInitialize();
            bool onUpdate();

            rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
            rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr info_pub_;
            rclcpp::Logger logger_;

            std::string camera_frame_id_;
            std::string output_encoding_;

            std::string device_file_;

            int fd_;

    };
} // namespace camera_driver

#endif // CAMERA_DRIVER