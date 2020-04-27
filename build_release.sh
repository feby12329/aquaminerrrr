set -ex
mkdir aquachain-miner # should not exist
# clean everything
make clean && make -C aquahash clean

# build each, copy to folder and clean afterwards
make -j 4 config=plain
mv bin/* aquachain-miner/
make clean && make -C aquahash clean
make -j 4 config=avx
mv bin/* aquachain-miner/
make clean && make -C aquahash clean
make -j 4 config=avx2
mv bin/* aquachain-miner/
make clean && make -C aquahash clean

tar czvf aquachain-miner-linux-amd64.tar.gz aquachain-miner/
ls -thalr aquachain-miner-linux-amd64.tar.gz
file aquachain-miner-linux-amd64.tar.gz
sha256sum aquachain-miner-linux-amd64.tar.gz
