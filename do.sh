cd threads
make clean
make
cd build
source ../../activate
pintos -- -q run priority-donate-chain
# pintos -- run priority-donate-chain
# pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single -l foo bar'
# pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'

# pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'