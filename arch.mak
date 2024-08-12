CCVER    := $(shell $(CC) -dumpversion | cut -f1-2 -d.)
ARCHNAME := $(shell $(CC) -dumpmachine)
