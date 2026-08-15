#!/bin/bash

sudo sysctl -w kernel.perf_event_paranoid=2

source ~/colcon_ws/install/setup.bash

ros2 launch camera_driver camera_driver.launch.py