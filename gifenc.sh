#!/bin/sh

palette="/tmp/palette.png"

#TODO(Vidar): output fps

ffmpeg -v warning -i /tmp/capture$1/frame%d.png -vf "palettegen" -y $palette
ffmpeg -v warning -thread_queue_size 512 -framerate 20 -i /tmp/capture$1/frame%d.png -i $palette -lavfi "paletteuse" -y /tmp/capture$1.gif
