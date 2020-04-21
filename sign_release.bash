set -ex
VERSION=$(cat VERSION)
if [ -z "$VERSION" ]; then
  VERSION=linux-x64-dev
fi
tar czpf aquachain-miner-$VERSION.tar.gz aquachain-miner
sha256sum aquachain-miner-$VERSION.tar.gz >> aquachain-miner-$VERSION.tar.gz.sha256sum
gpg --armor --detach-sign aquachain-miner-$VERSION.tar.gz
gpg --armor --detach-sign aquachain-miner-$VERSION.tar.gz.sha256sum

