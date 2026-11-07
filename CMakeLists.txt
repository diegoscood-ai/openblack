cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")

# VCPKG has an imperfect android implementation.
# NDK must be in env and vcpkg_android.cmake selects architecture.
if (VCPKG_TARGET_ANDROID)
  set(ENV{ANDROID_NDK_HOME} ${ANDROID_NDK_HOME})
  include(vcpkg_android)
endif ()

# On Visual Studio generators default to using vcpkg as the most newbie friendly option
if (CMAKE_GENERATOR MATCHES "^Visual Studio")
  message(
    STATUS
      "Defaulting to using vcpkg in a Windows build environment. You can disable with OPENBLACK_USE_VCPKG"
  )
  option(OPENBLACK_USE_VCPKG
         "Resolve dependencies using the vcpkg submodule toolchain" ON
  )
else ()
  option(OPENBLACK_USE_VCPKG
         "Resolve dependencies using the vcpkg submodule toolchain" OFF
  )
endif ()

# If using vcpkg and not manually specified the toolchain then set it for them
if (OPENBLACK_USE_VCPKG AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE
      ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake
      CACHE STRING "Default to vcpkg toolchain file"
  )
endif ()

# The repository patches some vcpkg ports, such as bgfx's Vulkan limits, wherever vcpkg is used, presets or not
# A build folder configured before the patches existed can hold an empty value, which would build without them
if (OPENBLACK_USE_VCPKG AND "${VCPKG_OVERLAY_PORTS}" STREQUAL "")
  set(VCPKG_OVERLAY_PORTS
      ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg-overlay-ports
      CACHE STRING "The repository's patched vcpkg ports" FORCE
  )
endif ()

# openblack's own Bink video decoder (components/bink): the game's films, and the debug GUI's Video window. Without it
# the films are skipped
option(OPENBLACK_BINK_DECODER
       "Build openblack's own Bink video decoder and play the game's films" ON
)
option(OPENBLACK_USE_PCH "Precompile the common third-party headers of openblack_lib" ON)

# vcpkg's install step takes most of every CMake run (40-60 s, even with nothing to install), and CMake runs again
# whenever a source is added. So it runs only when what it installs from changed since the last install in this build
# folder: the manifest, the repository's patched ports, the vcpkg checkout's baseline, the triplet or the compiler.
# Set OPENBLACK_VCPKG_INSTALL to ALWAYS to run it every time
set(OPENBLACK_VCPKG_INSTALL "CHANGED" CACHE STRING "When vcpkg installs the manifest: CHANGED or ALWAYS")
if (OPENBLACK_USE_VCPKG AND NOT OPENBLACK_VCPKG_INSTALL STREQUAL "ALWAYS")
  file(GLOB_RECURSE _openblack_vcpkg_inputs "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg-overlay-ports/*")
  list(SORT _openblack_vcpkg_inputs)
  list(PREPEND _openblack_vcpkg_inputs "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json")
  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/versions/baseline.json")
    list(APPEND _openblack_vcpkg_inputs "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/versions/baseline.json")
  endif ()
  set(_openblack_vcpkg_key_rest "$ENV{VCToolsVersion};$ENV{VSCMD_ARG_TGT_ARCH}")
  foreach (_file IN LISTS _openblack_vcpkg_inputs)
    file(SHA256 "${_file}" _hash)
    file(RELATIVE_PATH _name "${CMAKE_CURRENT_SOURCE_DIR}" "${_file}")
    string(APPEND _openblack_vcpkg_key_rest "\n${_name} ${_hash}")
  endforeach ()
  # a new build folder knows its triplet only after project(), so its stamp is written with the triplet chosen there
  set(_openblack_vcpkg_key
      "${VCPKG_TARGET_TRIPLET};${_openblack_vcpkg_key_rest}"
  )
  # a change to any of them makes the build run CMake again, as vcpkg's own step does for the manifest
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_openblack_vcpkg_inputs})
  set(_openblack_vcpkg_stamp "${CMAKE_BINARY_DIR}/openblack-vcpkg-install.stamp")
  set(_openblack_vcpkg_last "")
  if (EXISTS "${_openblack_vcpkg_stamp}" AND EXISTS "${CMAKE_BINARY_DIR}/vcpkg_installed/vcpkg/status")
    file(READ "${_openblack_vcpkg_stamp}" _openblack_vcpkg_last)
  endif ()
  if (_openblack_vcpkg_last STREQUAL _openblack_vcpkg_key)
    set(VCPKG_MANIFEST_INSTALL OFF)
    message(STATUS "vcpkg install skipped: nothing it installs from changed")
  else ()
    # the old stamp goes first: an install that fails or is stopped may already have removed packages, and the next
    # run must install again rather than skip
    file(REMOVE "${_openblack_vcpkg_stamp}")
    set(VCPKG_MANIFEST_INSTALL ON)
    set(_openblack_vcpkg_write_stamp ON)
  endif ()
endif ()

project(openblack VERSION 0.1.0)

if (_openblack_vcpkg_write_stamp AND Z_VCPKG_MANIFEST_INSTALL_RESULT EQUAL "0")
  file(WRITE "${_openblack_vcpkg_stamp}"
       "${VCPKG_TARGET_TRIPLET};${_openblack_vcpkg_key_rest}"
  )
endif ()

# Build memory (openblack, 2026-10-03): Ninja runs cores+2 jobs, and every link of the ~93 executables reads the whole
# static openblack_lib (~480 MB), so many links at once ran out of RAM. Links go through a pool of
# OPENBLACK_LINK_JOBS (other generators ignore job pools; this sets the JOB_POOLS property, so -DCMAKE_JOB_POOLS is
# not used).
set(OPENBLACK_LINK_JOBS 1 CACHE STRING "How many executables Ninja links at the same time")
if (NOT OPENBLACK_LINK_JOBS GREATER 0)
  set(OPENBLACK_LINK_JOBS 1)
endif ()
set_property(GLOBAL APPEND PROPERTY JOB_POOLS openblack_link_pool=${OPENBLACK_LINK_JOBS})
set(CMAKE_JOB_POOL_LINK openblack_link_pool)
# Build memory: every MSVC compile holds its own RAM, so at most OPENBLACK_COMPILE_JOBS at once, whatever -j says
set(OPENBLACK_COMPILE_JOBS 2 CACHE STRING "How many sources Ninja compiles at the same time")
if (NOT OPENBLACK_COMPILE_JOBS GREATER 0)
  set(OPENBLACK_COMPILE_JOBS 1)
endif ()
set_property(GLOBAL APPEND PROPERTY JOB_POOLS openblack_compile_pool=${OPENBLACK_COMPILE_JOBS})
set(CMAKE_JOB_POOL_COMPILE openblack_compile_pool)
# Reproducible MSVC outputs: no time stamps in the objects, libraries and executables, so two builds of one commit in
# one folder give byte-identical files, and an unchanged executable can be told by its bytes. Code generation is the
# same with or without it
if (MSVC)
  add_compile_options(/Brepro)
  add_link_options(/Brepro)
  string(APPEND CMAKE_STATIC_LINKER_FLAGS " /Brepro")
endif ()

# Configure C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# don't allow in source builds
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# do find_package with CONFIG before MODULE
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

# Output binaries to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

option(
  OPENBLACK_CLANG_TIDY_CHECKS
  "Do extra checks with clang-tidy if it's available (takes longer to build, enable for CI)"
  OFF
)
option(OPENBLACK_WARNINGS_AS_ERRORS
       "Treat warnings as errors (disable easier prototyping, enable for CI)"
       OFF
)
option(OPENBLACK_TRACE_TIME
       "Compilation Time analysis (only available with clang)" OFF
)

find_program(
  CLANG_TIDY NAMES clang-tidy-7 clang-tidy-6.0 clang-tidy-5.0 clang-tidy-4.0
                   clang-tidy
)

find_package(SDL2 CONFIG REQUIRED)

find_package(Stb QUIET)
if (NOT Stb_FOUND)
  find_path(
    Stb_INCLUDE_DIR
    NAMES stb_image_write.h stb_image.h stb_rect_pack.h
    PATHS /usr/include/stb REQUIRED
  )
endif ()

find_package(unofficial-minizip CONFIG QUIET)
if (TARGET unofficial::minizip::minizip)
  add_library(minizip::minizip ALIAS unofficial::minizip::minizip)
else ()
  find_package(PkgConfig)
  pkg_check_modules(minizip REQUIRED minizip IMPORTED_TARGET)
  add_library(minizip::minizip ALIAS PkgConfig::minizip)
  target_link_libraries(PkgConfig::minizip INTERFACE z ${minizip_LIBRARIES})
endif ()

# patch until next glm release
find_package(glm REQUIRED)
if (TARGET glm AND NOT TARGET glm::glm)
  add_library(glm::glm ALIAS glm)
endif ()

# vcpkg loses the dependency of bx on windows which fails to pull in all headers
find_package(bgfx REQUIRED)
if (TARGET bgfx::bgfx AND TARGET bgfx::bx)
  target_link_libraries(bgfx::bgfx INTERFACE bgfx::bx)
endif ()

find_package(OpenAL REQUIRED)
find_package(imgui REQUIRED)
find_package(spdlog 1.3.0 REQUIRED)
find_package(EnTT 3.7.0 CONFIG REQUIRED) # only available as a config
find_path(CXXOPTS_INCLUDE_DIRS "cxxopts.hpp")

include(ClangFormat)

# gets bundled dependencies (imgui, bgfx.cmake)
add_subdirectory(externals)
add_subdirectory(components/l3d)
add_subdirectory(components/pack)
add_subdirectory(components/lnd)
add_subdirectory(components/anm)
add_subdirectory(components/glw)
add_subdirectory(components/morph)
add_subdirectory(components/creaturemind)
add_subdirectory(components/psys)
add_subdirectory(components/rawimage)
add_subdirectory(components/cam)
add_subdirectory(components/edt)
add_subdirectory(components/gestures)
add_subdirectory(components/dance)
add_subdirectory(components/ScriptLibrary)
add_subdirectory(components/lhvmlang)
add_subdirectory(components/lhvmdecompiler)
add_subdirectory(components/lhvmcompiler)
add_subdirectory(components/helpstats)
add_subdirectory(components/gamesave)
# The precompiled header (OPENBLACK_USE_PCH): third-party and standard headers that most sources reach through
# Locator.h, ECS/Registry.h and the components; openblack_lib and every test group (with gtest) precompile it. No
# project headers: they change often, and a change would rebuild every source
set(OPENBLACK_PCH_HEADERS
    # std
    <cstddef>
    <cstdint>
    <cstdlib>
    <cstring>
    <cmath>
    <cassert>
    <algorithm>
    <array>
    <bit>
    <chrono>
    <filesystem>
    <functional>
    <limits>
    <map>
    <memory>
    <optional>
    <queue>
    <set>
    <source_location>
    <span>
    <string>
    <string_view>
    <tuple>
    <unordered_map>
    <unordered_set>
    <utility>
    <vector>
    # glm (the GLM_FORCE_* defines are target-wide, above)
    <glm/vec2.hpp>
    <glm/vec3.hpp>
    <glm/vec4.hpp>
    <glm/mat3x3.hpp>
    <glm/mat4x4.hpp>
    <glm/geometric.hpp>
    <glm/fwd.hpp>
    <glm/gtc/constants.hpp>
    <glm/gtc/quaternion.hpp>
    <glm/gtc/type_precision.hpp>
    # EnTT
    <entt/fwd.hpp>
    <entt/core/hashed_string.hpp>
    <entt/core/type_info.hpp>
    <entt/entity/entity.hpp>
    <entt/entity/helper.hpp>
    <entt/entity/registry.hpp>
    <entt/locator/locator.hpp>
    <entt/resource/cache.hpp>
    # logging
    <fmt/format.h>
    <spdlog/spdlog.h>
)

add_subdirectory(src)
add_subdirectory(apps/camtool)
add_subdirectory(apps/l3dtool)
add_subdirectory(apps/packtool)
add_subdirectory(apps/lndtool)
add_subdirectory(apps/anmtool)
add_subdirectory(apps/glwtool)
add_subdirectory(apps/morphtool)
add_subdirectory(apps/creaturemindtool)
add_subdirectory(apps/lhvmtool)
add_subdirectory(apps/savetool)

# Map CMAKE_HOST_SYSTEM_PROCESSOR value
if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64"
    OR ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64"
)
  set(OPENBLACK_HOST_ARCH "x64")
elseif (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86"
        OR ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "i386"
)
  set(OPENBLACK_HOST_ARCH "Win32")
elseif (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "ARM64")
  set(OPENBLACK_HOST_ARCH "ARM64")
else ()
  set(OPENBLACK_HOST_ARCH ${CMAKE_HOST_SYSTEM_PROCESSOR})
endif ()

# Cross compile check
if (CMAKE_CROSSCOMPILING)
  set(OPENBLACK_CROSSCOMPILING ON)
endif ()
if (MSVC)
  if (NOT "${OPENBLACK_HOST_ARCH}" STREQUAL "${CMAKE_VS_PLATFORM_NAME}"
      AND VCPKG_TARGET_TRIPLET
      AND NOT VCPKG_TARGET_TRIPLET MATCHES "${OPENBLACK_HOST_ARCH}*"
  )
    set(OPENBLACK_CROSSCOMPILING ON)
  endif ()
endif ()

if (NOT OPENBLACK_CROSSCOMPILING) # Requires python and running compiled apps
  add_subdirectory(test/mock)
endif ()

find_package(GTest CONFIG)
if (GTest_FOUND AND NOT OPENBLACK_CROSSCOMPILING)
  enable_testing()
  add_subdirectory(test)
endif ()

# Set openblack project as default startup project in Visual Studio
set_property(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT openblack
)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Determinism (openblack engine, docs/wiki/engine/floating-point.md#floating-point): the game logic must give the same
# floats on every build and thread, as the original's (its x87 FPU at 24 bits: one rounding per operation). Fused or
# reassociated float math changes them, so these flags stop the configure.
function(openblack_check_deterministic_fp)
  set(flags "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CMAKE_CXX_FLAGS_DEBUG}")
  foreach (target openblack_lib openblack)
    if (TARGET ${target})
      get_target_property(options ${target} COMPILE_OPTIONS)
      if (options)
        string(APPEND flags " ${options}")
      endif ()
    endif ()
  endforeach ()
  foreach (bad "/fp:fast" "/fp:contract" "/arch:AVX2" "/arch:AVX512" "-ffast-math" "-mfma" "-ffp-contract=fast")
    string(FIND "${flags}" "${bad}" at)
    if (NOT at EQUAL -1)
      message(FATAL_ERROR "${bad}: fused or reassociated float math changes the game's results (determinism, "
                          "docs/wiki/engine/floating-point.md#floating-point). Remove it.")
    endif ()
  endforeach ()
endfunction ()
openblack_check_deterministic_fp()
