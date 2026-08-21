#!/bin/sh
/usr/bin/ffmpeg -ss 00:00:00 -i "$1" -vf "select=eq(n\,50)" -vframes 1 "$2".jpg
#/usr/bin/ffmpeg -ss 00:00:03.333  -vf "select=eq(n\,99)" $1 $2
