cd vm
make clean
make
source ../activate
cd build
pintos --gdb --fs-disk=10 -p tests/vm/pt-grow-stack:pt-grow-stack --swap-disk=4 -- -q   -f run pt-grow-stack
