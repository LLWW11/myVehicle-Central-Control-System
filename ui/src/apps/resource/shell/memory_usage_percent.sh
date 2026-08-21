#!/bin/bash  

total_memory=$(grep MemTotal /proc/meminfo | awk '{print $2}')  
free_memory=$(grep MemAvailable /proc/meminfo | awk '{print $2}')  
used_memory=$((total_memory - free_memory))  
used_percentage=$(echo "scale=0; $used_memory * 100 / $total_memory" | bc)  
echo "${used_percentage}"
