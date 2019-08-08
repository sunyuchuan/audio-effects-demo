add_executable(test_all_effects test_all_effects.c)
target_link_libraries(test_all_effects audio_effect m  avutil swresample)

add_executable(test_echo test_echo.c)
target_link_libraries(test_echo audio_effect m pthread avutil swresample)

add_executable(test_effect test_effect.c)
target_link_libraries(test_effect audio_effect m pthread avutil swresample)

add_executable(test_equalizer test_equalizer.c)
target_link_libraries(test_equalizer audio_effect m pthread avutil swresample)

add_executable(test_fifo test_fifo.c)
target_link_libraries(test_fifo audio_effect m pthread avutil swresample)

add_executable(test_logger test_logger.c)
target_link_libraries(test_logger audio_effect m pthread avutil swresample)

add_executable(test_minions test_minions.c)
target_link_libraries(test_minions audio_effect m pthread avutil swresample)

add_executable(test_noise_suppression test_noise_suppression.c)
target_link_libraries(test_noise_suppression audio_effect m pthread avutil swresample)

add_executable(test_voice_morph test_voice_morph.c)
target_link_libraries(test_voice_morph audio_effect m  avutil swresample)

add_executable(test_xmly_audio_effects test_xmly_audio_effects.c)
target_link_libraries(test_xmly_audio_effects audio_effect m pthread avutil swresample)

add_executable(test_xmly_echo test_xmly_echo.c)
target_link_libraries(test_xmly_echo audio_effect m pthread avutil swresample)

add_executable(test_xmly_reverb test_xmly_reverb.c)
target_link_libraries(test_xmly_reverb audio_effect m pthread avutil swresample)