local_dir  := $(subdirectory)
local_pgm  := $(local_dir)/nditx
local_src  := $(wildcard $(local_dir)/*.cpp)
local_objs := $(call src_to_obj, $(local_src) $(ndi_common))

programs   += $(local_pgm)
sources    += $(local_src)

$(local_pgm): $(local_objs)
