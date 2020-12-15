# ROMACAR

This repository stores the control code of a high-frequency-force-controllable, four-individually-drivern-wheel prototype car.  

The project will make a validation 1:8 car first, then a beefier 1:5 one.

## 1:8 car

This car is controlled by a mini x86 PC, running ROS melodic(corresponding to Ubuntu 18.04).  

At the first stage, it will use the pc to reveice signals from radio remote controller, use a simple Arkman car model, and controls 4 DJI C620-M3508 power system, together with 2 Dynamixel XH430-W210-R Servos.


## How to use the code
### Motor communication setup
Every time you plug in the Cando module, please open ./catkin_ws/src/chasses in terminal, and run "expect MotorSetup.sh", this script will setup the Cando communication for you.