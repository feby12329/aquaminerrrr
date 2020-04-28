# first: prerequisites

```
apt install cmake libjsoncpp-dev libgmp-dev libc-ares-dev wget unzip file tree
```
### also some form of zlib headers, either zlib0g-dev or zlib1g-dev

```
apt install zlib1g-dev 
```

### use `zlib0g-dev` if not provided by your distro

# building

### Just type `make`

the makefile is set up so that it downloads libspdlog and libaquahash,
and downloads libcurl and builds the leanest version possible.

The libcurl configuration doesn't require or use openssl, so if you want that
you must add it yourself. Most pools just use http for worker communications
anyways.

The CMakelists.txt is under construction and can probably be used, but the
Makefile works and is tested with travis-ci.

## Configuration

Possible configs:

```
make config=plain
make config=avx
make config=avx2
```

**Note: you must `make clean` between building different configs.**

This is so that libaquahash is cleaned (which uses the avx instructions)

## scripts

If everything worked, you should have a ./bin directory with one or more static binaries. At this point, if you are creating a release you can run:

```
./build_release.sh
```

This builds all three flavors (plain, avx, avx2) and puts them in a .tar.gz.

After the tarball is created, the `sign_release.bash` script creates the
PGP signatures to go along with the release.

For information on modifying, please see the LICENSE file (GPLv3)



