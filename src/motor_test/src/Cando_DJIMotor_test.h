#include "ros/ros.h"
#include "std_msgs/String.h"

#include <sstream>

#define target_rpm -2500

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