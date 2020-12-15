#! /usr/bin/expect
spawn sudo ip link set can0 type can bitrate 1000000
expect "password for romacar:"
send "123456\r"
interact

spawn sudo ip link set up can0
expect "password for romacar:"
send "123456\r"
interact

