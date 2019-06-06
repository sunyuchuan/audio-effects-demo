#!/bin/bash
clear
rm -rf build
mkdir -p build/valgrind_log
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=1 ..
make

# valgrind --tool=callgrind ./test
# valgrind --leak-check=full --log-file=noise_suppression.log ./example
# kcachegrind callgrind.out* &

echo -e "\033[1;43;30m\ntest_all_effects...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_all_effects.log ./test_all_effects ../data/pcm_mono_44kHz_0035.pcm test_all_effects.pcm
echo -e "\033[1;43;30m\ntest_echo...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_echo.log ./test_echo ../data/pcm_mono_44kHz_0035.pcm test_echo.pcm
echo -e "\033[1;43;30m\ntest_effect...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_effect.log ./test_effect
echo -e "\033[1;43;30m\ntest_equalizer...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_equalizer.log ./test_equalizer ../data/pcm_mono_44kHz_0035.pcm test_equalizer.pcm
echo -e "\033[1;43;30m\ntest_fifo...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_fifo.log ./test_fifo
echo -e "\033[1;43;30m\ntest_logger...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_logger.log ./test_logger
echo -e "\033[1;43;30m\ntest_minions...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_minions.log ./test_minions ../data/pcm_mono_44kHz_0035.pcm test_minions.pcm
echo -e "\033[1;43;30m\ntest_noise_suppression...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_noise_suppression.log ./test_noise_suppression ../data/pcm_mono_44kHz_0035.pcm test_noise_suppression.pcm
echo -e "\033[1;43;30m\ntest_voice_morph...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_voice_morph.log ./test_voice_morph ../data/pcm_mono_44kHz_0035.pcm test_voice_morph.pcm
echo -e "\033[1;43;30m\ntest_xmly_audio_effects...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_xmly_audio_effects.log ./test_xmly_audio_effects ../data/pcm_mono_44kHz_0035.pcm test_xmly_audio_effects.pcm
echo -e "\033[1;43;30m\ntest_xmly_echo...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_xmly_echo.log ./test_xmly_echo ../data/pcm_mono_44kHz_0035.pcm test_xmly_echo.pcm
echo -e "\033[1;43;30m\ntest_xmly_reverb...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_xmly_reverb.log ./test_xmly_reverb ../data/pcm_mono_44kHz_0035.pcm test_xmly_reverb.pcm

cat valgrind_log/test_all_effects.log
echo -e "\n"
cat valgrind_log/test_echo.log
echo -e "\n"
cat valgrind_log/test_effect.log
echo -e "\n"
cat valgrind_log/test_equalizer.log
echo -e "\n"
cat valgrind_log/test_fifo.log
echo -e "\n"
cat valgrind_log/test_logger.log
echo -e "\n"
cat valgrind_log/test_minions.log
echo -e "\n"
cat valgrind_log/test_noise_suppression.log
echo -e "\n"
cat valgrind_log/test_voice_morph.log
echo -e "\n"
cat valgrind_log/test_xmly_audio_effects.log
echo -e "\n"
cat valgrind_log/test_xmly_echo.log
echo -e "\n"
cat valgrind_log/test_xmly_reverb.log
echo -e "\n"