NAME := aquachain-miner
VERSION := $(shell git rev-parse --short HEAD)
ifeq ($(VERSION),)
VERSION := "dev"
endif
VERSION := $(shell cat VERSION)-$(VERSION)
WD := $(PWD)
# path to curl lib (has ./bin/curl-config and ./lib/libcurl.a)
CURLDIR?=/tmp/curl
CXXFLAGS := -O3 -std=c++11 -pedantic -Wall -Werror -Iinclude -I. -Iaquahash/include -Ispdlog/include -I${CURLDIR}/include -pthread -static -DVERSION=\""$(VERSION)"\"
CFLAGS += -O3
SRCDIR := src
OBJDIR := _obj

# 'make config=avx2' to build avx2 version
suffix := -unknown
ifeq ($(config), avx2)
CFLAGS += -mavx2
suffix := -avx2
else ifeq ($(config), avx)
CFLAGS += -mavx
suffix := -avx
else ifeq ($(config), debug)
CFLAGS += -ggdb
suffix := -debug
else
CFLAGS := -march=native
suffix := -plain
endif
$(info Building: $(NAME)-$(VERSION)$(suffix))

# combine cflags
ALLFLAGS := $(CFLAGS) $(CXXFLAGS)

# static link aquahash and spdlog (others are installed with apt-get)
STATICLIBS := spdlog/libspdlog.a aquahash/libaquahash.a
LDFLAGS+=-ljsoncpp -lgmp $(STATICLIBS) $(shell ${CURLDIR}/bin/curl-config --static-libs) -static -lpthread

CPP_SOURCES := $(wildcard $(SRCDIR)/*.cpp)
#CPP_OBJECTS := $(CPP_SOURCES:.cpp)
CPP_OBJECTS := $(addprefix $(OBJDIR)/,$(notdir $(CPP_SOURCES:.cpp=.o)))
$(info Sources: $(CPP_SOURCES))
$(info Objects: $(CPP_OBJECTS))
bin/$(NAME)-$(VERSION)$(suffix): deps $(CPP_OBJECTS)
	mkdir -p bin
	@echo LINKING
	$(CXX) $(ALLFLAGS) -o $@ $(CPP_OBJECTS) $(LDFLAGS)

deps:	$(STATICLIBS) include/cli11/CLI11.hpp $(CURLDIR) 
.PHONY += deps
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(STATICLIBS) $(CURLDIR)
	@echo COMPILING $<
	@mkdir -p $(OBJDIR)
	$(CXX) $(ALLFLAGS) -c -o $@ $<

aquahash/libaquahash.a: aquahash
	$(info building aquahash: $(CFLAGS))
	env CFLAGS="$(CFLAGS)" $(MAKE) -C aquahash libaquahash.a

include/cli11/CLI11.hpp:
	$(info fetching CLI11 header)
	mkdir -p include/cli11/
	wget -O include/cli11/CLI11.hpp https://github.com/CLIUtils/CLI11/releases/download/v1.9.0/CLI11.hpp

clean:
	rm -vf src/*.o src/*.a
	rm -rvf bin
	rm -rvf $(OBJDIR)

aquahash:
	$(info fetching aquahash source)
	git clone https://github.com/aquachain/aquahash aquahash

spdlog/libspdlog.a: spdlog
	$(info building libspdlog.a)
	cd spdlog && cmake . && make -j2 spdlog

spdlog: 
	wget -O spdlog.zip https://github.com/gabime/spdlog/archive/v1.5.0.zip
	rm -rvf spdlog spdlog-*
	unzip spdlog.zip
	mv spdlog-1.5.0 spdlog
	rm spdlog.zip

distclean:
	$(MAKE) clean
	rm -rvf aquahash spdlog vend deps aquachain-miner bin include/cli11
	rm -rvf /tmp/curl


/tmp/curl:
	bash setup_libs.bash

debug:
	$(MAKE) config=debug
