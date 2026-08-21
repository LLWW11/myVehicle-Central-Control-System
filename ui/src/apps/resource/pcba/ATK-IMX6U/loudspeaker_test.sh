#!/bin/sh
gst-play-1.0 /usr/share/sounds/alsa/Front_Center.wav
amixer sset Speaker 127,127
for i in {1..1000}; do
        gst-play-1.0 /usr/share/sounds/alsa/Front_Center.wav
        sleep 1
done

