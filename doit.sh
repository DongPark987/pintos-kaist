# cd threads
cd userprog
make clean
make
cd build
source ../../activate
# pintos -- -q run priority-donate-chain

# pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'
# pintos --fs-disk=10 -p tests/userprog/args-null:args-null -- -q -f run 'args-null'
# pintos --fs-disk=10 -p tests/userprog/create-normal:create-normal -- -q -f run 'create-normal'
# pintos --fs-disk=10 -p tests/userprog/read-normal:read-normal -p ../../tests/userprog/sample.txt:sample.txt -- -q   -f run read-normal
# pintos --fs-disk=10 -p tests/userprog/close-twice:close-twice -p ../../tests/userprog/sample.txt:sample.txt -- -q   -f run close-twice
# pintos --fs-disk=10 -p tests/userprog/fork-once:fork-once -- -q -f run fork-once
# pintos  --fs-disk=10 -p tests/userprog/fork-multiple:fork-multiple -- -q -f run fork-multiple
# pintos  --fs-disk=10 -p tests/userprog/fork-recursive:fork-recursive -- -q -f run fork-recursive
# pintos  --fs-disk=10 -p tests/userprog/wait-killed:wait-killed -- -q -f run wait-killed
pintos --fs-disk=10 -p tests/userprog/exec-once:exec-once -p tests/userprog/child-simple:child-simple -- -q -f run exec-once