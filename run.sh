set -x

mkdir mnt

./jsonfs ./fs.json
# ./jsonfs -d -f ./mnt
# run on single thread
# ./jsonfs -d -s -f ./mnt
