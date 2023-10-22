# TeraRanger 40m ROS2 driver

[![Build](https://github.com/simutisernestas/teraranger_ros2/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/simutisernestas/teraranger_ros2/actions/workflows/build.yml)

This is a port of official driver https://github.com/Terabee/teraranger to ROS2. No other range sensor than 40m is supported yet.

## Setup

- Install serial driver:

```bash
sudo apt install ros-humble-serial-driver
```

- Clone and build the repo in ros2 ws

```bash
cd ros2_ws/src/
git clone https://github.com/simutisernestas/teraranger_ros2
cd ..
colcon build
```

## How to use it


```bash
ros2 run teraranger_ros2 teraranger_ros2_node [--ros-args -p portname:=/dev/ttyACMx]
```


> 📝 Default port: */dev/ttyACM0* 

## Debug

Check the topic:
```bash
ros2 topic echo /teraranger_evo_40m
```

Check the port in use:
```bash
rosparam get /teraranger_ros2_node portname
```