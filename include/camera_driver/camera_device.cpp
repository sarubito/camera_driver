#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/image_encodings.hpp"

#include "camera_driver.hpp"

namespace camera_driver
{
    CameraDriver::CameraDriver(rclcpp::NodeOptions const & options)
    : rclcpp::Node{"camera_driver", options}, logger_{this->get_logger()}
    {
        this->declare_parameter("device_file", "/dev/video0");

        if(!onInitialize()){
            RCLCPP_ERROR(logger_, "Failed Initialized");
        }
    }

    CameraDriver::~CameraDriver()
    {
        if(fd_ >= 0){
            ::close(fd_);
        }
    }

    bool CameraDriver::onInitialize()
    {
        bool ret = false;
        ret = onDeviceInitialize() && onParameterInitialize() 
            && onPublisherInitialize() &&onOpenCVInitialize();

        return ret;
    }

    bool CameraDriver::onDeviceInitialize()
    {
        bool ret = false;

        // ::はグローバル名前空間にある関数を呼び出すという意味
        fd_ = ::open(device_file_.c_str(), O_RDWR);
        RCLCPP_INFO(logger_, "open device : %s", device_file_.c_str());
        if(fd_ < 0){
            RCLCPP_ERROR(logger_, "failed opening device %s", device_file_.c_str());
            return ret;
        }

        struct v4l2_capability cap;
        // ここでデバイスのcapability(権限の組み合わせ)について問い合わせる
        // https://qiita.com/Brutus/items/37d942214b4c6edd08df
        if(ioctl(fd_, VIDIOC_QUERYCAP, &cap) == -1){
            RCLCPP_ERROR(logger_, "QUERYCAP");
            return ret;
        }

        // 対応しているかの確認
        if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
            RCLCPP_ERROR(logger_, "no video capture");
            return ret;
        }

        if(!(cap.capabilities & V4L2_CAP_STREAMING)){
            RCLCPP_ERROR(logger_, "does not support stream");
            return ret;
        }

        return ret;
    }

    bool CameraDriver::onParameterInitialize()
    {
        bool ret = false;

        device_file_ = this->get_parameter("device_file").as_string();

        ret = true;
        return ret;
    }

    bool CameraDriver::onPublisherInitialize()
    {
        bool ret = false;

        image_pub_ = this->create_publisher<sensor_msgs::msg::Image>("image_raw", 10);
        info_pub_ = this->create_publisher<sensor_msgs::msg::CameraInfo>("camera_info", 10);

        ret = true;
        return ret;
    }

    bool CameraDriver::onOpenCVInitialize()
    {
        bool ret = false;

        ret = true;
        return ret;
    }

    bool CameraDriver::onUpdate()
    {
        bool ret = false;

        ret = true;
        return ret;
    }

} // namespace camera_driver