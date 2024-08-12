ifeq ($(CC),)
override CC := gcc
else ifeq ($(CC),cc)
override CC := gcc
endif

ifeq ($(CXX),)
override CXX := g++
endif

ifeq ($(AR),)
override AR := gcc-ar
endif

ifeq ($(RANLIB),)
override RANLIB := gcc-ranlib
endif

STRIP  ?= strip
PKG    ?= pkg-config
MKDIR  := mkdir -p
CP     := cp
MV     := mv -f
RM     := rm
LN     := ln
SHELL  := /bin/bash
ECHO   := @echo -e
