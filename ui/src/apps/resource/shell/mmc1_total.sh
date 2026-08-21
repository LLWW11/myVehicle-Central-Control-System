#!/bin/bash  

echo $(fdisk -l /dev/mmcblk1 | grep "Disk /dev/mmcblk1:" | awk '{print $3$4}')  
