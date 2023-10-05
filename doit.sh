# cd threads
cd userprog
make clean
make
cd build
source ../../activate
# pintos -- -q run priority-donate-chain

# pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'
# pintos --fs-disk=10 -p tests/userprog/args-multiple:args-multiple -- -q -f run 'args-multiple some arguments for you!'
pintos --fs-disk=10 -p tests/userprog/create-null:create-null -- -q -f run 'create-null'
# pintos --fs-disk=10 -p tests/userprog/create-bad-ptr:create-bad-ptr -- -q -f run 'create-bad-ptr'
