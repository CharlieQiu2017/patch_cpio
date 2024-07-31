CXXFLAGS=-Wall -Wextra -pedantic -Werror -Wfatal-errors
ifeq ($(debug),1)
CXXFLAGS+=-g
else
CXXFLAGS+=-Os -march=native -fomit-frame-pointer -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none
endif

all: patch_cpio

patch_cpio: main.cc
	$(CXX) $^ -o $@ $(CXXFLAGS)

clean:
	rm -f patch_cpio

.PHONY: all clean
