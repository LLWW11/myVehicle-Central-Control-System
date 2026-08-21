#!/bin/sh
amixer cset name='Differential Mux' 'Line 2'
amixer cset name='Left Line Mux' 'Line 2L'
amixer cset name='Right Line Mux' 'Line 2R'

amixer cset name='Left Mixer Left Playback Switch' 'off'
amixer cset name='Right Mixer Right Playback Switch' 'off'

for i in {1..1000}; do
	echo "当前状态正在录音，请讲话..."
	#arecord -r 44100 -f S16_LE -d 5 .record.wav
	#amixer cset name='Left Mixer Left Playback Switch' 'off'
	#amixer cset name='Right Mixer Right Playback Switch' 'off'
	arecord -f cd -d 5 .record.wav
	echo "当前状态正在播放，请听..."
	amixer cset name='Left Mixer Left Playback Switch' 'on'
        amixer cset name='Right Mixer Right Playback Switch' 'on'
	aplay .record.wav
done

