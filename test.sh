#!/bin/bash
#
#
aplay -l
aplay -v -D hw:0,0 -f S16_LE  -c 2 -r 48000 -t raw /dev/zero -d 2
arecord -v -D hw:0,0 -f S16_LE -c 2 -r 48000 -t raw -d 2 /dev/null
  
