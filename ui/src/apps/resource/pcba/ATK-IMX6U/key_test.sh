#!/bin/sh
key=/dev/input/$(grep -A 10 "gpio-keys" /proc/bus/input/devices | awk -F'=' '/Handlers/ {split($2, a, " "); for(i in a) if (a[i] ~ /^event/) print a[i]}')
evtest $key
