# The openblack mod API for native mods: one C header (include/openblack/mod_api.h), nothing to build. openblack uses it
# to load native mods (src/Mods/Native) and every native mod includes it (mods/examples).
add_library(modsdk INTERFACE)
target_include_directories(modsdk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_sources(modsdk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/openblack/mod_api.h)
