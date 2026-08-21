#!/bin/bash  

#echo $(grep MemTotal /proc/meminfo | awk '{print $2$3}')  
echo $(free -h | grep "Mem:" |awk '{print $2}')  
