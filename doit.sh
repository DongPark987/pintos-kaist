# For thread
cd threads
make clean
make
cd build
source ../../activate
pintos -- -q run priority-donate-multiple2
# pintos -- -q run priority-donate-chain
# pintos -- -q -mlfqs run mlfqs-nice-10
# pintos -- -q -mlfqs run mlfqs-fair-2