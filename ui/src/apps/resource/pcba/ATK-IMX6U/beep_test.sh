#!/bin/sh
echo "蜂鸣器会响一秒后停止"
echo 1 > /sys/class/leds/beep/brightness
sleep 1
echo 0 > /sys/class/leds/beep/brightness 
