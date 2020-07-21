add_executable(test_beautify test_beautify.c)
target_link_libraries(test_beautify audio_effects m pthread)

add_executable(test_fifo test_fifo.c)
target_link_libraries(test_fifo audio_effects m pthread)

add_executable(test_logger test_logger.c)
target_link_libraries(test_logger audio_effects m pthread)

add_executable(test_minions test_minions.c)
target_link_libraries(test_minions audio_effects m pthread)

add_executable(test_noise_suppression test_noise_suppression.c)
target_link_libraries(test_noise_suppression audio_effects m pthread)

add_executable(test_voice_morph test_voice_morph.c)
target_link_libraries(test_voice_morph audio_effects m pthread)

add_executable(test_xmly_audio_effects test_xmly_audio_effects.c)
target_link_libraries(test_xmly_audio_effects audio_effects m pthread)

add_executable(test_xmly_echo test_xmly_echo.c)
target_link_libraries(test_xmly_echo audio_effects m pthread)

add_executable(test_xmly_reverb test_xmly_reverb.c)
target_link_libraries(test_xmly_reverb audio_effects m pthread)
