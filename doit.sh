# cd threads
cd userprog
make clean
make
cd build
source ../../activate
# pintos -- -q run priority-donate-chain

# pintos --gdb --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'
pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'