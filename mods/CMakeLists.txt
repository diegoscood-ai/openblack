# The example mods that come with openblack: the modpack mods/examples (off by default), copied next to the exe in
# Mods/examples, with its native mods built into their bin/ folders. A native mod links nothing of openblack: only the
# C header of components/modsdk (openblack passes its functions in at load time).

if (NOT TARGET openblack)
  return() # Android: no exe, no Mods folder
endif ()
# next to the exe, whatever the generator (bin/<config> with multi-config ones, bin with the others)
set(OPENBLACK_MODS_OUTPUT "$<TARGET_FILE_DIR:openblack>/Mods")

# openblack_add_native_mod(<target> <mod id> <folder under Mods/> <sources...>)
function (OPENBLACK_ADD_NATIVE_MOD TARGET ID FOLDER)
  add_library(${TARGET} MODULE ${ARGN})
  target_link_libraries(${TARGET} PRIVATE modsdk)
  set_target_properties(
    ${TARGET}
    PROPERTIES PREFIX ""
               OUTPUT_NAME ${ID}
               LIBRARY_OUTPUT_DIRECTORY "${OPENBLACK_MODS_OUTPUT}/${FOLDER}/bin"
               RUNTIME_OUTPUT_DIRECTORY "${OPENBLACK_MODS_OUTPUT}/${FOLDER}/bin"
               C_VISIBILITY_PRESET hidden
               FOLDER "mods"
  )
  add_dependencies(openblack_example_mods ${TARGET})
endfunction ()

add_custom_target(
  openblack_example_mods ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/examples ${OPENBLACK_MODS_OUTPUT}/examples
  COMMENT "Copying mods/examples to Mods/examples"
)
set_property(TARGET openblack_example_mods PROPERTY FOLDER "mods")

openblack_add_native_mod(
  example_native_hello example.native-hello examples/example.native-hello
  examples/example.native-hello/src/hello.c
)
openblack_add_native_mod(
  example_native_library example.native-library examples/example.native-library
  examples/example.native-library/src/library.c
)
target_include_directories(example_native_library PRIVATE examples/example.native-library/include)
openblack_add_native_mod(
  example_native_consumer example.native-consumer examples/example.native-consumer
  examples/example.native-consumer/src/consumer.c
)
# the consumer uses the library's public header, as any mod using a library mod would
target_include_directories(example_native_consumer PRIVATE examples/example.native-library/include)
