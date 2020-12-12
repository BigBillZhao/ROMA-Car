// #include <chrono>
#include <iostream>
#include <fstream>
#include <thread>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "Cando_DJIMotor_test.h"

struct can_frame rxframe;
struct can_frame txframe;

Motor_Control_Info Motor_info = {0, 0, 0, 0, 0, 0, 0, 0};

// open network, configure socket CAN, return file descriptor
int socketCANSetUp(void)
{
  int s;
  int nbytes;
  struct sockaddr_can addr;
  struct ifreq ifr;
  socklen_t len = sizeof(addr); // for recvfrom
  const char *ifname = "can0";

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

  return s;
}

void canRx(const struct can_frame *raw, Motor_Control_Info *processed_data, int s)
{
  int  nbytes = read(s, &rxframe, sizeof(struct can_frame));
  if (nbytes < 0)
  {
    perror("Read");
    return;
  }
  processed_data->angle_now = (uint16_t)(raw->data[0] << 8 | raw->data[1]);
  processed_data->rpm_now = (uint16_t)(raw->data[2] << 8 | raw->data[3]);
  processed_data->current_now = (uint16_t)(raw->data[4] << 8 | raw->data[5]);
  ROS_INFO("omega:%d torque:%d ",
         processed_data->rpm_now, processed_data->current_now);
}

// speed loop PID, calculate current needed
int16_t speedPID(int desired_rpm, Motor_Control_Info *info)
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

int canTx(int16_t Current_sent, int s)
{
  /* waiting to add other motor */
  txframe.data[Tx_C610_2_High] = Current_sent >> 8; // done: negative with type convertion
  txframe.data[Tx_C610_2_Low] = Current_sent & 255;
  int nbytes = write(s, &txframe, sizeof(struct can_frame));
  return nbytes;
}

void commandCallback(const Motor_Control_Info &msg, int s)
{
  canRx(&rxframe, &Motor_info, s);
  int16_t Current_sent = speedPID(target_rpm, &Motor_info);
  canTx(Current_sent, s);
}

int main(int argc, char **argv)
{
  int s = socketCANSetUp();

  ros::init(argc,argv,"MotorServoController");
  ros::NodeHandle n;
  ros::Subscriber sub = n.subscribe("actuator_info", 10, boost::bind(commandCallback,_1,s));

  ros::Rate r(1000);
  while (ros::ok())
  {
    ros::spinOnce();;
    r.sleep();
  }
  const char* set_down = "sudo ip link set down can0";
  system(set_down);

  return 0;
}

