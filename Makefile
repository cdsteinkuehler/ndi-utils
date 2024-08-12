include compiler.mak
include arch.mak
include common.mak

# $(call src_to_obj, source_file_list)
src_to_obj = $(addprefix $(OBJDIR)/, $(subst .cpp,.o,$(filter %.cpp,$1)))

# $(subdirectory)
subdirectory = $(patsubst %/module.mk,%, \
	$(word $(words $(MAKEFILE_LIST)), \
	$(MAKEFILE_LIST)))

OBJDIR := objs

# Make sure ndi_common is the first module...
modules      := ndi_common

# ...then auto-detect all other subdirectories with a module.mk file
modules      += $(filter-out ndi_common, $(subst /module.mk,,$(wildcard */module.mk)))

# Collect information from each module in these four variables.
# Initialize them here as simple variables.
programs     :=
sources      :=
ndi_common   :=
extra_clean  :=

# Generate objs and deps from sources
objs = $(addprefix $(OBJDIR)/, $(subst .cpp,.o,$(filter %.cpp,$(sources))))
deps = $(subst .o,.d,$(objs))

# List of all objs directories
OBJDIRS := $(addprefix $(OBJDIR)/, $(modules))

# Remove OBJDIR when cleaning
extra_clean += $(OBJDIR)

# Add in library dependencies
override LDLIBS += -ldl -lpthread -lndi -lexplain

# Build dependency files anytime we generate a .o file
DEPFLAGS = -MT $@ -MMD -MP -MF $(OBJDIR)/$*.Td
POSTCOMPILE = $(MV) $(OBJDIR)/$*.Td $(OBJDIR)/$*.d && touch $@

override CXXFLAGS += -std=c++14
override LDFLAGS  += -static-libgcc -static-libstdc++ -Wl,-no-allow-shlib-undefined -Wl,-z,defs

# Make sure all is the default target
all:

# Include module.mk files from subdirectories
include $(addsuffix /module.mk,$(modules))

# The actual target for all, now that we have a list of programs
.PHONY: all
all: $(programs)

.PHONY: clean
clean:
	$(Q)$(RM) -rf $(objs) $(programs) $(libraries) $(deps) $(extra_clean)

.PHONY: install
install: $(programs)
	$(Q)sudo install -g root -o root -m755 $^ /usr/local/bin/

# Don't fail if any dependencies don't exist
$(deps):

# Include existing dependency files if we're not going to delete them
ifneq "$(MAKECMDGOALS)" "clean"
include $(wildcard $(deps))
endif

# Create any non-existing object directories
$(OBJDIRS):
	$(Q)$(MKDIR) $@

# Depend on the $OBJDIR/<module> directory so it gets created if needed
.SECONDEXPANSION:
$(OBJDIR)/%.o : %.cpp $(OBJDIR)/%.d | $$(@D)
ifneq ($(Q),)
	$(ECHO) "CXX\t$<"
endif
	$(Q)$(CXX) -c $(DEPFLAGS) $(CXXFLAGS) -o $@ $<
	$(Q)$(POSTCOMPILE)

$(programs):
ifneq ($(Q),)
	$(ECHO) "CXX\t$@"
endif
	$(Q)$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)
ifeq ($(NO_STRIP),)
ifeq ($(DEBUG),)
ifneq ($(Q),)
	$(ECHO) "STRIP\t$@"
endif
	-$(Q)$(STRIP) --strip-debug --strip-unneeded $@
endif
endif
