#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "ros/ros.h"
#include "std_msgs/String.h"
#include "dynamixel_sdk/dynamixel_sdk.h" 

/*
Parameters below is for XC-430-W240
*/

#define ADDR_PRO_TORQUE_ENABLE          64
#define ADDR_PRO_GOAL_POSITION          116
#define ADDR_PRO_PRESENT_POSITION       132
#define LEN_PRO_GOAL_POSITION           4
#define LEN_PRO_PRESENT_POSITION        4
#define PROTOCOL_VERSION                2.0
#define DXL_ID                          1 
#define BAUDRATE                        57600
#define DEVICENAME                      "/dev/ttyUSB0"
#define TORQUE_ENABLE                   1
#define TORQUE_DISABLE                  0 
#define DXL_MINIMUM_POSITION_VALUE      0 
#define DXL_MAXIMUM_POSITION_VALUE      1023
#define DXL_MOVING_STATUS_THRESHOLD     5
#define INIT_VAL                        512

#define Kp_speed 2.6
#define Ki_speed 0.8
#define Kd_speed 0.8
#define Kp_T 1.50
#define Ki_T 0.40
#define Kd_T 0.10
#define RpmIntegralSaturation 600 // highly influence the wave

#define CAN1 0x200
#define CAN2 0x1FF

#define Tx_C610_2_High 2
#define Tx_C610_2_Low 3
#define MaxCurrent 10000
#define MinCurrent -10000

// Rx data frame
#define Rx_C610_2 0x202
#define Angle_High 0
#define Angle_Low 1
#define Omega_High 2
#define Omega_Low 3
#define Current_High 4
#define Current_Low 5

typedef struct
{
    // last_time for error
    int16_t rpm_last_time;
    int16_t rpm_now;
    int integral_rpm;

    int16_t current_last_time;
    int16_t current_now;

    int16_t angle_last_time;
    int16_t angle_now;
    int integral_angle;
} Motor_Control_Info;

struct can_frame rxframe;
struct can_frame txframe;

dynamixel::PortHandler *portHandler = dynamixel::PortHandler::getPortHandler(DEVICENAME);
dynamixel::PacketHandler *packetHandler = dynamixel::PacketHandler::getPacketHandler(PROTOCOL_VERSION);
int dxl_comm_result = COMM_TX_FAIL;
uint8_t dxl_error = 0;                      
int32_t dxl_present_position = 0;   

Motor_Control_Info Motor_info = {0, 0, 0, 0, 0, 0, 0, 0};

void rxProcess(const struct can_frame *raw, Motor_Control_Info *processed_data)
{
  processed_data->angle_now = (uint16_t)(raw->data[0] << 8 | raw->data[1]);
  processed_data->rpm_now = (uint16_t)(raw->data[2] << 8 | raw->data[3]);
  processed_data->current_now = (uint16_t)(raw->data[4] << 8 | raw->data[5]);
  printf("omega:%d torque:%d ",
         processed_data->rpm_now, processed_data->current_now);
}

// speed loop PID, calculate current needed
int16_t CurrentToSend(int desired_rpm, Motor_Control_Info *info)
{
  // out-loop for rpm
  int delta_rpm = desired_rpm - info->rpm_now;    // Kp
  int diff_rpm = delta_rpm - info->rpm_last_time; //Kd
  info->integral_rpm += abs(info->integral_rpm + delta_rpm) < RpmIntegralSaturation ? delta_rpm : 0;
  int desired_current = (Kp_speed * delta_rpm + Ki_speed * info->integral_rpm + Kd_speed * diff_rpm);

  // update error last time
  info->rpm_last_time = delta_rpm;

  if (desired_current > 10000)
    return (int16_t)10000;
  else if (desired_current < -10000)
    return (int16_t)-10000;
  else
    return (int16_t)desired_current;
}


int s;
int nbytes;
struct sockaddr_can addr;
struct ifreq ifr;
socklen_t len = sizeof(addr); // for recvfrom
const char *ifname = "can0";
int count = 0;
int16_t Current_sent = 0;

void chatterCallback(const std_msgs::String::ConstPtr& msg)
{
  ROS_INFO("I heard: [%s]", msg->data.c_str());
  int speed = stoi(msg->data);
  int pos = speed;
  speed -= 200;
  dxl_comm_result = packetHandler->write4ByteTxRx(portHandler, DXL_ID, ADDR_PRO_GOAL_POSITION, pos * 2, &dxl_error);
  if (dxl_comm_result != COMM_SUCCESS)
  {
    printf("%s\n", packetHandler->getTxRxResult(dxl_comm_result));
  }
  else if (dxl_error != 0)
  {
    printf("%s\n", packetHandler->getRxPacketError(dxl_error));
  }
//   while(1)
//   {
    printf("no.%d ", count);
    count++;
    nbytes = read(s, &rxframe, sizeof(struct can_frame));
    if (nbytes < 0)
    {
      perror("Read");
    //   break;
    }

    rxProcess(&rxframe, &Motor_info);

    Current_sent = CurrentToSend(speed, &Motor_info);

    txframe.data[Tx_C610_2_High] = Current_sent >> 8; // done: negative with type convertion
    txframe.data[Tx_C610_2_Low] = Current_sent & 255;
    nbytes = write(s, &txframe, sizeof(struct can_frame));

    if (nbytes < 0)
    {
      perror("Write");
    //   break;
    }
//   }
}

int main(int argc, char **argv)
{
  if (portHandler->openPort())
  {
    printf("Succeeded to open the port!\n");
  }
  else
  {
    printf("Failed to open the port!\n");
    return 0;
  }
  if (portHandler->setBaudRate(BAUDRATE))
  {
    printf("Succeeded to change the baudrate!\n");
  }
  else
  {
    printf("Failed to change the baudrate!\n");
    return 0;
  }
  dxl_comm_result = packetHandler->write1ByteTxRx(portHandler, DXL_ID, ADDR_PRO_TORQUE_ENABLE, TORQUE_ENABLE, &dxl_error);
  if (dxl_comm_result != COMM_SUCCESS)
  {
    printf("%s\n", packetHandler->getTxRxResult(dxl_comm_result));
  }
  else if (dxl_error != 0)
  {
    printf("%s\n", packetHandler->getRxPacketError(dxl_error));
  }
  else
  {
    printf("Dynamixel has been successfully connected \n");
  }
  dxl_comm_result = packetHandler->write4ByteTxRx(portHandler, DXL_ID, ADDR_PRO_GOAL_POSITION, INIT_VAL, &dxl_error);
  if (dxl_comm_result != COMM_SUCCESS)
  {
    printf("%s\n", packetHandler->getTxRxResult(dxl_comm_result));
  }
  else if (dxl_error != 0)
  {
    printf("%s\n", packetHandler->getRxPacketError(dxl_error));
  }
  /* set up can network in ubuntu */   
  const char* set_bitrate = "sudo ip link set can0 type can bitrate 1000000";
  const char* set_up = "sudo ip link set up can0";
  if(system(set_bitrate)||system(set_up))
    printf("set up network fail");

  /* open the socket */
  if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
  {
    perror("Error while opening socket");
    return -1;
  }

  /* get interface name from the received CAN frame */
  strcpy(ifr.ifr_name, ifname);
  ioctl(s, SIOCGIFINDEX, &ifr);

  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;

  /* bind socket to the can interface */
  if (bind(s, (struct sockaddr *)&addr, len) < 0)
  {
    perror("Error in socket bind!!!");
    return -2;
  }

  /* configure CAN info and fresh the data */
  txframe.can_id = CAN1;
  txframe.can_dlc = 8;
  for (int i = 0; i < 8; i++)
    txframe.data[i] = 0;
  nbytes = write(s, &txframe, sizeof(struct can_frame));

  ros::init(argc, argv, "listener");
  ros::NodeHandle n;
  ros::Subscriber sub = n.subscribe("chatter", 1000, chatterCallback);
  ros::spin();

  dxl_comm_result = packetHandler->write1ByteTxRx(portHandler, DXL_ID, ADDR_PRO_TORQUE_ENABLE, TORQUE_DISABLE, &dxl_error);
  if (dxl_comm_result != COMM_SUCCESS)
  {
    printf("%s\n", packetHandler->getTxRxResult(dxl_comm_result));
  }
  else if (dxl_error != 0)
  {
    printf("%s\n", packetHandler->getRxPacketError(dxl_error));
  }
  portHandler->closePort();

  const char* set_down = "sudo ip link set down can0";
  system(set_down);
  return 0;
}