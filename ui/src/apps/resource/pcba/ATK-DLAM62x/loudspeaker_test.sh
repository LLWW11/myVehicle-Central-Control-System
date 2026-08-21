#!/bin/sh
gst-play-1.0 /home/root/shell/audio/test.mp3
for i in {1..1000}; do
	gst-play-1.0 /home/root/shell/audio/test.mp3
	sleep 1
done
