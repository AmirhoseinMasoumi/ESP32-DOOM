# PrBoom WAD Tables Component
# Pre-computed lookup tables for DOOM

idf_component_register(
    SRCS 
        "SINETABL.c"
        "GAMMATBL.c"
        "TANGTABL.c"
        "TANTOANG.c"
    INCLUDE_DIRS "include"
)
