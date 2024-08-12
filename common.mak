# This is probably gcc/g++
CXX11    := $(shell echo $(CCVER) \> 4.6 | bc)
CXX11GNU := $(shell echo $(CCVER) \> 4.8 | bc)

# Enable C99 for the C compiler
override CFLAGS += -std=c99

# Use the appropriate flag for enabling C++11 support; NEEDED!
ifeq ($(CXX11),1)
override CXXFLAGS += -std=c++11
else
override CXXFLAGS += -std=c++0x
endif

# Add some more flags
override CFLAGS   += -pedantic -Wall
override CXXFLAGS += -pedantic -Wall

# Allow the compiler to use pipes rather than temporary files
override CFLAGS   += -pipe
override CXXFLAGS += -pipe

# Default compiler flags unless overridden on make's command-line
ifneq ($(DEBUG),)
override CFLAGS   += -O0 -g -D_DEBUG
override CXXFLAGS += -O0 -g -D_DEBUG
else
# With at least GCC 5.4, these two optimizations appear to optimize out
# crucial portions of the code.
override CFLAGS   += -O3 -DNDEBUG -fno-ipa-cp-clone -fno-inline-functions
override CXXFLAGS += -O3 -DNDEBUG -fno-ipa-cp-clone -fno-inline-functions

# When debugging an optimized build, having the frame pointers intact can help
ifneq ($(NO_OMIT_FRAME_POINTER),)
override CFLAGS   += -fno-omit-frame-pointer
override CXXFLAGS += -fno-omit-frame-pointer
endif
endif

# Weird GCC 4.7 thing...
ifneq ($(CXX11GNU),1)
override CXXFLAGS += -D_GLIBCXX_USE_NANOSLEEP -D_GLIBCXX_USE_SCHED_YIELD
endif

# Ensure symbols are hidden by default
override CFLAGS   += -fvisibility=hidden
override CXXFLAGS += -fvisibility=hidden

# Default linker flags unless overridden on make's command-line
override LDFLAGS ?= -Wl,--as-needed

# Should commands be verbose or do pretty output?
ifndef Q
ifeq ($(V),1)
	Q =
else
	Q = @
endif
export Q
endif
