# cd threads
cd userprog
make clean
make
cd build
source ../../activate
# pintos -- -q -mlfqs run mlfqs-load-60
# pintos -- -q run priority-donate-chain
# pintos --gdb -- run priority-donate-chain
# pintos -- -q run priority-donate-
# pintos --fs-disk=10 -p tests/userprog/args-single:args-single --gdb -- -f run args-single onearg
pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -f -q run 'args-single onearg'