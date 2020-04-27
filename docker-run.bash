apt update
apt install -y sudo git libjsoncpp-dev zlib1g-dev build-essential wget unzip cmake libc-ares-dev libgmp-dev file tree
cd /src && bash .travis.bash && mv *.tar.gz /release/


