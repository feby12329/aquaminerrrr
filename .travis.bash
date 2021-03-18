# build deps, then build targets
set -ex
uname -a
cat /etc/os-release
# kind of not important anymore
#sudo apt remove libssl-dev -y
#sudo apt install curl ca-certificates wget tree -y
sudo apt install -y git libjsoncpp-dev zlib1g-dev build-essential wget unzip cmake libc-ares-dev libgmp-dev file tree
# build curl lite
bash setup_libs.bash

# build spdlog 
make deps

# build 3 binaries (plain, avx, avx2)
./build_release.sh

file aquachain-miner*
file aquachain-miner/*
aquachain-miner/aquachain-miner-*plain -h

# make sure it can hash without illegal instruction
#aquachain-miner/aquachain-miner-*plain -B 

#mkdir build && cd build && cmake .. && make -j8 && file aquachain-miner*


