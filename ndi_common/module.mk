local_dir  := $(subdirectory)
local_src  := $(wildcard $(local_dir)/*.cpp)

sources    += $(local_src)
ndi_common += $(local_src)
