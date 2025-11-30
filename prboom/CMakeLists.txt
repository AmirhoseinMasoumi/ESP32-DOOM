# PrBoom DOOM Engine Component
file(GLOB PRBOOM_SOURCES "*.c")

# Exclude disabled files
list(FILTER PRBOOM_SOURCES EXCLUDE REGEX ".*\\.disabled$")
list(FILTER PRBOOM_SOURCES EXCLUDE REGEX ".*md5\\.c\\.x$")

idf_component_register(
    SRCS ${PRBOOM_SOURCES}
    INCLUDE_DIRS "." 
    REQUIRES esp_timer esp_partition fatfs prboom-wad-tables
)

# Compiler options for prboom - suppress warnings from legacy code
# This is old C code that needs many warnings disabled to compile cleanly
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Os
    -DHAVE_CONFIG_H
    -DESP32
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-unused-but-set-parameter
    -Wno-unused-value
    -Wno-implicit-fallthrough
    -Wno-maybe-uninitialized
    -Wno-char-subscripts
    -Wno-unused-const-variable
    -Wno-missing-field-initializers
    -Wno-stringop-truncation
    -Wno-format-truncation
    -Wno-format-overflow
    -Wno-misleading-indentation
    -Wno-pointer-sign
    -Wno-type-limits
    -Wno-int-to-pointer-cast
    -Wno-parentheses
    -Wno-address
    -Wno-duplicate-decl-specifier
    -Wno-nonnull
    -Wno-sizeof-pointer-div
)
