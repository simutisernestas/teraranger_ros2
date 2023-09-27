# TeraRanger 40m ROS2 driver

[![Build](https://github.com/simutisernestas/teraranger_ros2/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/simutisernestas/teraranger_ros2/actions/workflows/build.yml)

This is a port of official driver https://github.com/Terabee/teraranger to ROS2. No other range sensor than 40m is supported yet.

## Setup

The only dependence is on serial driver:

```bash
sudo apt install ros-humble-serial-driver
```

Build it with CMake:

```bash
mkdir build && cd build
cmake ..
make
export SERIAL_DEVICE="/dev/ttyACM0"
./teraranger_ros2_node $SERIAL_DEVICE
```

Check the topic:
```bash
ros2 topic echo /teraranger_evo_40m
```
