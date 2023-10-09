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
# pintos --fs-disk=10 -p tests/userprog/create-empty:create-empty -- -q   -f run create-empty
# pintos --fs-disk=10 -p tests/userprog/create-bad-ptr:create-bad-ptr -- -q   -f run create-bad-ptr
# pintos --fs-disk=10 -p tests/userprog/open-missing:open-missing -- -q   -f run open-missing
# pintos --gdb --fs-disk=10 -p tests/userprog/open-normal:open-normal -p ../../tests/userprog/sample.txt:sample.txt -- -q   -f run open-normal
# pintos --fs-disk=10 -p tests/userprog/read-normal:read-normal -p ../../tests/userprog/sample.txt:sample.txt -- -q   -f run read-normal
# pintos  --fs-disk=10 -p tests/userprog/fork-once:fork-once -- -q   -f run fork-once
# pintos --fs-disk=10 -p tests/userprog/rox-simple:rox-simple -- -q   -f run rox-simple
# pintos --fs-disk=10 -p tests/userprog/rox-child:rox-child -p tests/userprog/child-rox:child-rox -- -q   -f run rox-child
# pintos --fs-disk=10 -p tests/userprog/exec-once:exec-once -p tests/userprog/child-simple:child-simple -- -q   -f run exec-once
# pintos --fs-disk=10 -p tests/filesys/base/syn-read:syn-read -p tests/filesys/base/child-syn-read:child-syn-read -- -q   -f run syn-read
# pintos --fs-disk=10 -p tests/filesys/base/syn-read:syn-read -p tests/filesys/base/child-syn-read:child-syn-read -- -q   -f run syn-read
# pintos --fs-disk=10 -p tests/userprog/no-vm/multi-oom:multi-oom -- -q   -f run multi-oom
# pintos --fs-disk=10 -p tests/userprog/bad-write:bad-write -p tests/userprog/bad-write:bad-write -- -q   -f run bad-write

pintos  --fs-disk=10 -p tests/userprog/dup2/dup2-complex:dup2-complex -p ../../tests/userprog/dup2/sample.txt:sample.txt -- -q   -f run dup2-complex
# pintos  --fs-disk=10 -p tests/userprog/dup2/dup2-simple:dup2-simple -p ../../tests/userprog/dup2/sample.txt:sample.txt -- -q   -f run dup2-simple

# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/fork-multiple:fork-multiple -- -q   -f run fork-multiple
# pintos  --fs-disk=10 -p tests/userprog/read-stdout:read-stdout -- -q   -f run read-stdout