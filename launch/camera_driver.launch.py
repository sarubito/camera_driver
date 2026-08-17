from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    pkg_share = get_package_share_directory('camera_driver')
    params_file = os.path.join(pkg_share, 'param', 'camera_driver_params.yaml')

    # camera_node = Node(
    #     package='camera_driver',
    #     executable='main_node',
    #     name='camera_driver',
    #     prefix=['nsys profile --sample=cpu --trace=cuda,nvtx,osrt --output=/home/ubuntu/nsight_profile/report1 --force-overwrite true'],
    #     output='screen',
    #     parameters=[params_file]
    # )

    camera_node = Node(
        package='camera_driver',
        executable='main_node',
        name='camera_driver',
        # prefix=['ncu -o /home/ubuntu/camera_node_profile'],
        output='screen',
        parameters=[params_file]
    )

    return LaunchDescription([camera_node])
