set -x
gcc -Wall jsonfs.c $(pkg-config fuse json-c --cflags --libs) -I./include -o jsonfs
