cd userprog
make clean
make
cd build
source ../../activate
# pintos -- -q run priority-donate-chain
# pintos -- run priority-donate-chain
# pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single -l foo bar'
# pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'

# pintos --fs-disk=10 -p tests/userprog/args-single:args-single -- -q -f run 'args-single onearg'
# pintos --fs-disk=10 -p tests/userprog/open-missing:open-missing -- -q   -f run open-missing
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/open-normal:open-normal -p ../../tests/userprog/sample.txt:sample.txt -- -q   -f run open-normal
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/open-twice:open-twice -p ../../tests/userprog/sample.txt:sample.txt -- -q   -f run open-twice

# pintos --fs-disk=10 -p tests/userprog/fork-once:fork-once -p ../../tests/userprog/sample.txt:sample.txt -- -q   -f run read-normal

# pintos -v -k -T 60 -m 20 --fs-disk=10 -p tests/userprog/fork-once:fork-once -- -q   -f run fork-once
# pintos -v -k -T 60 -m 20 --fs-disk=10 -p tests/userprog/rox-simple:rox-simple -- -q   -f run rox-simple
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/fork-read:fork-read -p ../../tests/userprog/sample.txt:sample.txt -- -q   -f run fork-read
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/wait-simple:wait-simple -p tests/userprog/child-simple:child-simple -- -q   -f run wait-simple
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/exec-once:exec-once -p tests/userprog/child-simple:child-simple -- -q   -f run exec-once
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/bad-write:bad-write -p tests/userprog/bad-write:bad-write -- -q   -f run bad-write

# pintos --fs-disk=10 -p tests/userprog/no-vm/multi-oom:multi-oom -- -q   -f run multi-oom

# pintos -v -k -T 60 -m 20  --fs-disk=10 -p tests/userprog/fork-multiple:fork-multiple -- -q   -f run fork-multiple
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/filesys/base/syn-write:syn-write -p tests/filesys/base/child-syn-wrt:child-syn-wrt -- -q   -f run syn-write


# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/create-bad-ptr:create-bad-ptr -- -q   -f run create-bad-ptr

pintos -v -k -T 300 -m 20   --fs-disk=10 -p tests/filesys/base/syn-read:syn-read -p tests/filesys/base/child-syn-read:child-syn-read -- -q   -f run syn-read 