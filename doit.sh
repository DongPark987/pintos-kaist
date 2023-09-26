# For thread
cd threads
make clean
make
cd build
source ../../activate
pintos -- -q run alarm-multiple
# pintos -- -q -mlfqs run mlfqs-load-1