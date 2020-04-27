# build deps, then build targets
set -ex
uname -a
cat /etc/os-release
# kind of not important anymore
#sudo apt remove libssl-dev -y
#sudo apt install curl ca-certificates wget tree -y
sudo apt install -y wget tree file 

# build curl lite
bash setup_libs.bash

# build spdlog 
make deps

# build 3 binaries (plain, avx, avx2)
./build_release.sh

file aquachain-miner*
file aquachain-miner/*
aquachain-miner/aquachain-miner-*plain -h

#mkdir build && cd build && cmake .. && make -j8 && file aquachain-miner*


