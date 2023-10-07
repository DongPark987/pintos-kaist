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

# pintos --fs-disk=10 -p tests/userprog/fork-once:fork-once -p ../../tests/userprog/sample.txt:sample.txt -- -q   -f run read-normal

# pintos -v -k -T 60 -m 20 --fs-disk=10 -p tests/userprog/fork-once:fork-once -- -q   -f run fork-once
pintos -v -k -T 60 -m 20 --fs-disk=10 -p tests/userprog/exec-missing:exec-missing -- -q   -f run exec-missing
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/fork-read:fork-read -p ../../tests/userprog/sample.txt:sample.txt -- -q   -f run fork-read
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/wait-simple:wait-simple -p tests/userprog/child-simple:child-simple -- -q   -f run wait-simple
# pintos -v -k -T 60 -m 20   --fs-disk=10 -p tests/userprog/exec-once:exec-once -p tests/userprog/child-simple:child-simple -- -q   -f run exec-once
