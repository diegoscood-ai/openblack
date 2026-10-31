# The mod SDK's version: a hash of every SDK-facing header (src/ModSdk), compiled into both the game and the mod loader
# library, so the two agree only when they were built from the same SDK. Any change to those headers moves it; a game
# whose SDK moved without the library being rebuilt with it starts unmodded.
#
# The headers are hashed by name and content, sorted by name, with their line endings made the same, so a checkout on
# any platform gives the same version. The header is written only when the version changes, so that nothing recompiles
# otherwise.
#
# Usage: cmake -DSDK_DIR=<src/ModSdk> -DOUTPUT=<header> -P GenerateModSdkVersion.cmake

file(GLOB headers RELATIVE "${SDK_DIR}" "${SDK_DIR}/*.h")
list(SORT headers)
set(all "")
foreach (header IN LISTS headers)
  file(READ "${SDK_DIR}/${header}" text)
  string(REPLACE "\r" "" text "${text}")
  string(APPEND all "${header}\n${text}\n")
endforeach ()
string(SHA256 version "${all}")

set(content
    "// Generated at build time by CMakeModules/GenerateModSdkVersion.cmake from the SDK-facing headers (src/ModSdk):\n"
)
string(APPEND content "// do not edit. The game and the mod loader library compile the same value.\n")
string(APPEND content "#pragma once\n\n")
string(APPEND content "inline constexpr char k_ModSdkVersion[] = \"${version}\";\n")

set(old "")
if (EXISTS "${OUTPUT}")
  file(READ "${OUTPUT}" old)
endif ()
if (NOT old STREQUAL content)
  file(WRITE "${OUTPUT}" "${content}")
endif ()
