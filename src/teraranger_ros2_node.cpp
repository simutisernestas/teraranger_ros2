#include "serial_driver/serial_driver.hpp"
#include <rclcpp/rclcpp.hpp>
#include "sensor_msgs/msg/range.hpp"

namespace sd = drivers::serial_driver;

#define OUT_OF_RANGE_VALUE -1
#define TOO_CLOSE_VALUE 0
#define INVALID_MEASURE 1
#define VALUE_TO_METER_FACTOR 0.001
#define EVO_40M_MIN 0.5
#define EVO_40M_MAX 40.0
#define SERIAL_SPEED 115200
#define SERIAL_TIMEOUT_MS 1000

static const char ENABLE_CMD[5] = {(char)0x00, (char)0x52, (char)0x02, (char)0x01, (char)0xDF};
static const char TEXT_MODE[4] = {(char)0x00, (char)0x11, (char)0x01, (char)0x45};
static const char BINARY_MODE[4] = {(char)0x00, (char)0x11, (char)0x02, (char)0x4C};
static const uint8_t BUFFER_SIZE = 4;
const float FOV = 0.0349066f;

static const uint8_t crc_table[] = {0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38, 0x3f, 0x36, 0x31, 0x24, 0x23,
                                    0x2a, 0x2d, 0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65, 0x48, 0x4f, 0x46, 0x41,
                                    0x54, 0x53, 0x5a, 0x5d, 0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5, 0xd8, 0xdf,
                                    0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd, 0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85,
                                    0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd, 0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc,
                                    0xd5, 0xd2, 0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea, 0xb7, 0xb0, 0xb9, 0xbe,
                                    0xab, 0xac, 0xa5, 0xa2, 0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a, 0x27, 0x20,
                                    0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32, 0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
                                    0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42, 0x6f, 0x68, 0x61, 0x66, 0x73, 0x74,
                                    0x7d, 0x7a, 0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c, 0xb1, 0xb6, 0xbf, 0xb8,
                                    0xad, 0xaa, 0xa3, 0xa4, 0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec, 0xc1, 0xc6,
                                    0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4, 0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c,
                                    0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44, 0x19, 0x1e, 0x17, 0x10, 0x05, 0x02,
                                    0x0b, 0x0c, 0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34, 0x4e, 0x49, 0x40, 0x47,
                                    0x52, 0x55, 0x5c, 0x5b, 0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63, 0x3e, 0x39,
                                    0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b, 0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
                                    0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb, 0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d,
                                    0x84, 0x83, 0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb, 0xe6, 0xe1, 0xe8, 0xef,
                                    0xfa, 0xfd, 0xf4, 0xf3};

std::atomic<float> measured_range(0.0f);

uint8_t crc8(uint8_t *p, uint8_t len)
{
  uint16_t i;
  uint16_t crc = 0x0;

  while (len--)
  {
    i = (crc ^ *p++) & 0xFF;
    crc = (crc_table[i] ^ (crc << 8)) & 0xFF;
  }
  return crc & 0xFF;
}

float two_chars_to_float(uint8_t c1, uint8_t c2)
{
  int16_t current_range = c1 << 8;
  current_range |= c2;

  float res = (float)current_range;
  return res;
}

void serial_port_callback(std::vector<uint8_t> &buffer, const size_t &bytes_transferred)
{
  if (bytes_transferred != 4)
  {
    printf("drop 4");
    return;
  }

  if (buffer[0] != 'T')
  {
    printf("drop T");
    return;
  }

  uint8_t crc = crc8(buffer.data(), 3);
  if (crc != buffer[3])
  {
    printf("crs missmatch");
    return;
  }

  int16_t range = buffer[1] << 8;
  range |= buffer[2];

  float final_range = range * VALUE_TO_METER_FACTOR;

  if (range == TOO_CLOSE_VALUE) // Too close, 255 is for short range
  {
    final_range = -std::numeric_limits<float>::infinity();
  }
  else if (range == OUT_OF_RANGE_VALUE) // Out of range
  {
    final_range = std::numeric_limits<float>::infinity();
  }
  else if (range == INVALID_MEASURE) // Cannot measure
  {
    final_range = std::numeric_limits<float>::quiet_NaN();
  }
  else if (final_range > EVO_40M_MAX)
  {
    final_range = std::numeric_limits<float>::infinity();
  }
  else if (final_range < EVO_40M_MIN)
  {
    final_range = -std::numeric_limits<float>::infinity();
  }

  measured_range.store(final_range);
}

int main(int argc, char **argv)
{
  const uint32_t baud_rate = SERIAL_SPEED;

  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("teraranger_ros2_node");

  node->declare_parameter("portname", "/dev/ttyACM0");
  rclcpp::Parameter port_param = node->get_parameter("portname");
  const std::string device = port_param.as_string();;

  const size_t threads_count = 1;
  auto io_context = std::make_unique<drivers::common::IoContext>(threads_count);
  auto serial_config = std::make_unique<sd::SerialPortConfig>(baud_rate, sd::FlowControl::NONE,
                                                              sd::Parity::NONE, sd::StopBits::ONE);
  auto serial_driver = std::make_unique<sd::SerialDriver>(*io_context);

  try
  {
    serial_driver->init_port(device, *serial_config);
    serial_driver->port()->open();
    serial_driver->port()->async_receive(*serial_port_callback);
  }
  catch (const std::exception &e)
  {
    RCLCPP_ERROR(node->get_logger(), "Error while opening serial port: %s", e.what());
    return 1;
  }

  auto range_pub = node->create_publisher<sensor_msgs::msg::Range>("teraranger_evo_40m", 10);

  // create timer running at 240 Hz
  auto timer = node->create_wall_timer(std::chrono::milliseconds(5), [&]()
                                       {
    sensor_msgs::msg::Range msg;
    msg.header.stamp = node->now();
    msg.header.frame_id = "teraranger_evo_40m";
    msg.radiation_type = sensor_msgs::msg::Range::INFRARED;
    msg.field_of_view = FOV;
    msg.min_range = EVO_40M_MIN;
    msg.max_range = EVO_40M_MAX;
    msg.range = measured_range.load();
    range_pub->publish(msg); });

  // executor
  rclcpp::executors::StaticSingleThreadedExecutor executor;
  executor.add_node(node);
  executor.spin();

  serial_driver->port()->close();
  rclcpp::shutdown();
  return 0;
}
