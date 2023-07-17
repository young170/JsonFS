# JsonFS: Json File System using FUSE 
* ITP30002-02
* Seongbin Kim 22100113

## Introduction
This homework asks you to write a FUSE program that constructs a user-
level file system based on the structure and data defined in a JSON file
- FUSE is a framework to handle file-related system calls by calling 
corresponding handlers defined in a user-level program

## Instructions
* Edit the .json file in the **run.sh** file.
```
./jsonfs <input_filename>
```
### Build
```
$ ./build.sh
+++ pkg-config fuse json-c --cflags --libs
++ gcc -Wall jsonfs.c -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse -I/usr/include json-c -lfuse -pthread -ljson-c -I./include -o jsonfs
```
### Run
```
$ ./run.sh
++ mkdir mnt
++ ./jsonfs ./fs.json
```

## Demonstration
### YouTube
[JsonFS_YouTube](https://youtu.be/fvu4JE_4Veo)
