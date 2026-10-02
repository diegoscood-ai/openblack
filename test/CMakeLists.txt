include(CTest)

# Macro for setting up and adding a simple test.
# TEST_NAME is the name of the test.
# TEST_SOURCE is the source file of the test.
macro (OPENBLACK_SETUP_AND_ADD_TEST TEST_NAME TEST_SOURCE)
  add_executable(${TEST_NAME} ${TEST_SOURCE})
  add_dependencies(${TEST_NAME} generate_mock_game_data)
  target_link_libraries(${TEST_NAME} PRIVATE GTest::gtest_main openblack_lib)
  target_compile_definitions(
    ${TEST_NAME} PRIVATE TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
                         GLM_ENABLE_EXPERIMENTAL
  )
  set_property(TARGET ${TEST_NAME} PROPERTY FOLDER "tests")
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endmacro ()

# Macro for setting up a test that uses JSON scenario files for validation.
# This macro does everything the standard test setup does, but also copies a
# directory of JSON scenarios into the build directory and adds a dependency on
# these scenarios.
# The TEST_NAME and TEST_SOURCE arguments have the same purpose as in the standard setup.
# The test should be in its own directory
macro (OPENBLACK_SETUP_AND_ADD_JSON_TEST TEST_NAME TEST_SOURCE)
  get_filename_component(TEST_DIR_NAME ${TEST_SOURCE} DIRECTORY)
  add_custom_target(
    ${TEST_NAME}_scenarios
    COMMAND
      ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR_NAME}/scenarios
      ${CMAKE_CURRENT_BINARY_DIR}/${TEST_DIR_NAME}/scenarios
  )
  set_property(TARGET ${TEST_NAME}_scenarios PROPERTY FOLDER "tests")
  openblack_setup_and_add_test(${TEST_NAME} ${TEST_SOURCE})
  target_include_directories(
    ${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party
                         vector_compare.h
  )
  add_dependencies(${TEST_NAME} ${TEST_NAME}_scenarios)
endmacro ()

openblack_setup_and_add_test(test_game_initialize test_game_initialize.cpp)
openblack_setup_and_add_test(test_load_scene test_load_scene.cpp)
openblack_setup_and_add_test(test_fixed test_fixed.cpp)
openblack_setup_and_add_test(test_zoomer test_zoomer.cpp)
openblack_setup_and_add_test(test_lh_matrix test_lh_matrix.cpp)
openblack_setup_and_add_test(test_land_normal test_land_normal.cpp)
openblack_setup_and_add_test(test_magic_tables test_magic_tables.cpp)
openblack_setup_and_add_test(test_spell_chants test_spell_chants.cpp)
openblack_setup_and_add_test(test_influence test_influence.cpp)
openblack_setup_and_add_test(test_spell_sounds test_spell_sounds.cpp)
target_link_libraries(test_spell_sounds PRIVATE pack)
openblack_setup_and_add_test(test_weather test_weather.cpp)
openblack_setup_and_add_test(test_food_wood test_food_wood.cpp)
openblack_setup_and_add_test(test_gestures test_gestures.cpp)
openblack_setup_and_add_test(test_teleport test_teleport.cpp)
openblack_setup_and_add_test(test_spell_forest test_spell_forest.cpp)
openblack_setup_and_add_test(test_lightning test_lightning.cpp)
target_link_libraries(test_lightning PRIVATE lnd)
openblack_setup_and_add_test(test_shield test_shield.cpp)
target_link_libraries(test_shield PRIVATE pack l3d bgfx::bgfx)
openblack_setup_and_add_test(test_explosion test_explosion.cpp)
openblack_setup_and_add_test(test_billboard test_billboard.cpp)
openblack_setup_and_add_test(test_frame_anim test_frame_anim.cpp)
openblack_setup_and_add_test(test_zsorter test_zsorter.cpp)
openblack_setup_and_add_test(test_land_morph test_land_morph.cpp)
openblack_setup_and_add_test(test_render_modes test_render_modes.cpp)
openblack_setup_and_add_test(test_heal test_heal.cpp)
openblack_setup_and_add_test(test_worship test_worship.cpp)
openblack_setup_and_add_test(test_villager_core test_villager_core.cpp)
openblack_setup_and_add_test(test_villager_decide test_villager_decide.cpp)
openblack_setup_and_add_test(test_water test_water.cpp)
openblack_setup_and_add_test(test_flock test_flock.cpp)
openblack_setup_and_add_test(test_storm test_storm.cpp)
openblack_setup_and_add_test(test_fire test_fire.cpp)
openblack_setup_and_add_test(test_water_queries test_water_queries.cpp)
openblack_setup_and_add_test(test_sea_cells test_sea_cells.cpp)
target_link_libraries(test_sea_cells PRIVATE lnd)
openblack_setup_and_add_test(test_sea_rows test_sea_rows.cpp)
openblack_setup_and_add_test(test_map_coords test_map_coords.cpp)
openblack_setup_and_add_test(test_gutils_distance test_gutils_distance.cpp)
openblack_setup_and_add_test(test_gutils_angle test_gutils_angle.cpp)
openblack_setup_and_add_test(test_object_metrics test_object_metrics.cpp)
openblack_setup_and_add_test(test_game_clock test_game_clock.cpp)
openblack_setup_and_add_test(test_land_light test_land_light.cpp)
openblack_setup_and_add_test(test_haze_land_light test_haze_land_light.cpp)
openblack_setup_and_add_test(test_sky_type test_sky_type.cpp)
openblack_setup_and_add_test(test_script_camera test_script_camera.cpp)
openblack_setup_and_add_test(test_script_camera_dual test_script_camera_dual.cpp)
openblack_setup_and_add_test(test_lh3d_colour test_lh3d_colour.cpp)
openblack_setup_and_add_test(test_argb4444 test_argb4444.cpp)
openblack_setup_and_add_test(test_rgb16 test_rgb16.cpp)
openblack_setup_and_add_test(test_bik_file test_bik_file.cpp)
openblack_setup_and_add_test(test_video_player test_video_player.cpp)
openblack_setup_and_add_test(test_sea_pass test_sea_pass.cpp)
openblack_setup_and_add_test(test_audio_laws test_audio_laws.cpp)
openblack_setup_and_add_test(test_psys_water test_psys_water.cpp)
target_link_libraries(test_psys_water PRIVATE lnd)
openblack_setup_and_add_test(test_land_raycast test_land_raycast.cpp)
target_link_libraries(test_land_raycast PRIVATE lnd)
openblack_setup_and_add_test(test_set_camera_pos camera/test_set_camera_pos.cpp)
openblack_setup_and_add_json_test(
  test_mobile_wall_hug mobile_wall_hug/test_mobile_wall_hug.cpp
)
openblack_setup_and_add_json_test(test_camera camera/test_camera.cpp)
openblack_setup_and_add_test(test_audio_tables test_audio_tables.cpp)
target_link_libraries(test_audio_tables PRIVATE pack)
openblack_setup_and_add_test(test_music_bank test_music_bank.cpp)
openblack_setup_and_add_test(test_music_engine test_music_engine.cpp)
openblack_setup_and_add_test(test_music_stream test_music_stream.cpp)
openblack_setup_and_add_test(test_game_music test_game_music.cpp)
target_link_libraries(test_game_music PRIVATE pack)
openblack_setup_and_add_test(test_voice_table test_voice_table.cpp)
target_link_libraries(test_voice_table PRIVATE pack)
openblack_setup_and_add_test(test_help_system test_help_system.cpp)
target_link_libraries(test_help_system PRIVATE pack)
openblack_setup_and_add_test(test_sample_play test_sample_play.cpp)
target_link_libraries(test_sample_play PRIVATE pack l3d anm glw lnd morph ScriptLibrary)
openblack_setup_and_add_test(test_sound_tags test_sound_tags.cpp)
target_link_libraries(test_sound_tags PRIVATE pack l3d anm glw lnd morph ScriptLibrary)
openblack_setup_and_add_test(test_script_sound test_script_sound.cpp)
target_link_libraries(test_script_sound PRIVATE pack l3d anm glw lnd morph ScriptLibrary)
openblack_setup_and_add_test(test_voices test_voices.cpp)
target_link_libraries(test_voices PRIVATE pack l3d anm glw lnd morph ScriptLibrary)
openblack_setup_and_add_test(test_anim_effects test_anim_effects.cpp)
target_link_libraries(test_anim_effects PRIVATE pack l3d anm glw lnd morph ScriptLibrary)
openblack_setup_and_add_test(test_mods test_mods.cpp)
target_link_libraries(test_mods PRIVATE pack l3d anm glw lnd morph ScriptLibrary)
openblack_setup_and_add_test(test_ui_sfx test_ui_sfx.cpp)
target_link_libraries(test_ui_sfx PRIVATE pack l3d anm glw lnd morph ScriptLibrary)
openblack_setup_and_add_test(test_guidance test_guidance.cpp)
target_link_libraries(test_guidance PRIVATE pack l3d anm glw lnd morph ScriptLibrary)
openblack_setup_and_add_test(test_spooky_voices test_spooky_voices.cpp)
target_link_libraries(test_spooky_voices PRIVATE pack l3d anm glw lnd morph ScriptLibrary)
