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
# pintos --fs-disk=10 -p tests/userprog/args-single:args-single --gdb -- -f run 'args-single onearg'
# pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -f -q run 'args-single onearg'
# pintos --fs-disk=10 -p tests/userprog/args-dbl-space:args-dbl-space -- -q   -f run 'args-dbl-space two  spaces!'
# pintos --fs-disk=10 -p tests/userprog/create-normal:create-normal -- -q   -f run create-normal
# pintos --fs-disk=10 -p tests/userprog/create-bad-ptr:create-bad-ptr -- -q   -f run create-bad-ptr
# pintos --fs-disk=10 -p tests/userprog/open-missing:open-missing -- -q   -f run open-missing
pintos --fs-disk=10 -p tests/userprog/open-normal:open-normal -p ../../tests/userprog/sample.txt:sample.txt -- -q   -f run open-normal