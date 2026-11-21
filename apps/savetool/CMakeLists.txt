set(SAVETOOL savetool.cpp)

source_group(apps\\savetool FILES ${SAVETOOL})

add_executable(savetool ${SAVETOOL})

target_link_libraries(savetool PRIVATE gamesave)

if (OPENBLACK_CLANG_TIDY_CHECKS)
  if (CLANG_TIDY)
    set_target_properties(savetool PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY})
  else ()
    message("Clang-tidy checks requested but unavailable")
  endif ()
endif ()

if (MSVC)
  target_compile_definitions(savetool PRIVATE _HAS_EXCEPTIONS=0)
  target_compile_options(savetool PRIVATE /W4 /WX /EHs- /EHc-)
else ()
  target_compile_options(
    savetool PRIVATE -Wall -Wextra -pedantic -Werror -fno-exceptions
  )
endif ()

set_property(TARGET savetool PROPERTY FOLDER "tools")
