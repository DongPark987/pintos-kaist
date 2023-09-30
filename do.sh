cd threads
make clean
make
cd build
source ../../activate
pintos -- -q -mlfqs run mlfqs-load-1
