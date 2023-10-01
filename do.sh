cd threads
make clean
make
cd build
source ../../activate
pintos -- -q -mlfqs run mlfqs-load-avg
# pintos -- -q run alarm-priority
