set -ex
gcc glsl_videoplayer.c  -o glsl_videoplayer `pkg-config --libs --cflags raylib`
./glsl_videoplayer
