#!/bin/bash
set -e

# Wrapper script intended to be run with sudo:
# sudo ./tools/profile_gpu_kernel_root.sh

source /opt/ros/jazzy/setup.bash
source /home/ubuntu/colcon_ws/install/setup.bash
export PATH="/usr/local/cuda/bin:$PATH"
export LD_LIBRARY_PATH="/usr/local/cuda/lib64:$LD_LIBRARY_PATH"
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST
export CYCLONEDDS_URI=file:///home/ubuntu/cyclonedds.xml

exec ros2 launch camera_driver camera_driver.launch.py
