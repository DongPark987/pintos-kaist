cd threads
make clean
make
cd build
source ../../activate
# pintos -- -q -mlfqs run mlfqs-load-60
pintos -- -q run priority-donate-chain
# pintos -- -q run priority-donate-nest