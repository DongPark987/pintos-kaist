# cd threads
cd vm
make clean
make
cd build
source ../../activate

# pintos   --fs-disk=10 -p tests/userprog/args-none:args-none --swap-disk=4 -- -q   -f run args-none
# pintos --gdb   --fs-disk=10 -p tests/userprog/args-none:args-none --swap-disk=4 -- -q   -f run args-none
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/vm/pt-grow-stack:pt-grow-stack --swap-disk=4 -- -q   -f run pt-grow-stack
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/vm/pt-grow-stack:pt-grow-stack --swap-disk=4 -- -q   -f run pt-grow-stack

# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/read-normal:read-normal -p ../../tests/userprog/sample.txt:sample.txt --swap-disk=4 -- -q   -f run read-normal
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/vm/pt-write-code:pt-write-code --swap-disk=4 -- -q   -f run pt-write-code
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/vm/pt-write-code2:pt-write-code2 -p ../../tests/vm/sample.txt:sample.txt --swap-disk=4 -- -q   -f run pt-write-code2
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/read-boundary:read-boundary -p ../../tests/userprog/sample.txt:sample.txt --swap-disk=4 -- -q   -f run read-boundary
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/fork-once:fork-once --swap-disk=4 -- -q   -f run fork-once
pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/wait-simple:wait-simple -p tests/userprog/child-simple:child-simple --swap-disk=4 -- -q   -f run wait-simple
