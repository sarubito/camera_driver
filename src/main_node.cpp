#include <iostream>

#include "rclcpp/rclcpp.hpp"
#include "camera_driver/camera_driver.hpp"

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);

    rclcpp::executors::SingleThreadedExecutor executor;
    // rclcpp::executors::MultiThreadedExecutor executor;

    rclcpp::Node::SharedPtr camera_driver_node = std::make_shared<camera_driver::CameraDriver>(rclcpp::NodeOptions());

    executor.add_node(camera_driver_node);
    executor.spin();

    rclcpp::shutdown();
}
