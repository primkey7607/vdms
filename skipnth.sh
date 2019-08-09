#! /bin/sh
BASEDIR=$(pwd)
ffmpeg -i $1 -vf "select=not(mod(n\,$2))" -vsync vfr img_%04d.png

