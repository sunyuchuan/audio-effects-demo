#!/bin/bash

work_path=$(dirname $0)
cd ${work_path}

clear
rm -rf build
mkdir -p build/valgrind_log
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=1 ..
make

echo -e "\033[1;43;30m\ntest_beautify...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_beautify.log ./tests/test_beautify ../data/pcm_mono_44kHz_0035.pcm test_beautify.pcm
echo -e "\033[1;43;30m\ntest_fifo...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_fifo.log ./tests/test_fifo
echo -e "\033[1;43;30m\ntest_logger...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_logger.log ./tests/test_logger
echo -e "\033[1;43;30m\ntest_minions...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_minions.log ./tests/test_minions ../data/pcm_mono_44kHz_0035.pcm test_minions.pcm
echo -e "\033[1;43;30m\ntest_noise_suppression...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_noise_suppression.log ./tests/test_noise_suppression ../data/pcm_mono_44kHz_0035.pcm test_noise_suppression.pcm
echo -e "\033[1;43;30m\ntest_voice_morph...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_voice_morph.log ./tests/test_voice_morph ../data/pcm_mono_44kHz_0035.pcm test_voice_morph.pcm
echo -e "\033[1;43;30m\ntest_xm_audio_effects...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_xm_audio_effects.log ./tests/test_xm_audio_effects ../data/pcm_mono_44kHz_0035.pcm test_xm_audio_effects.pcm
echo -e "\033[1;43;30m\ntest_xmly_echo...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_xmly_echo.log ./tests/test_xmly_echo ../data/pcm_mono_44kHz_0035.pcm test_xmly_echo.pcm
echo -e "\033[1;43;30m\ntest_xmly_reverb...\033[0m"
valgrind --leak-check=full --log-file=valgrind_log/test_xmly_reverb.log ./tests/test_xmly_reverb ../data/pcm_mono_44kHz_0035.pcm test_xmly_reverb.pcm

cat valgrind_log/test_beautify.log
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
cat valgrind_log/test_xm_audio_effects.log
echo -e "\n"
cat valgrind_log/test_xmly_echo.log
echo -e "\n"
cat valgrind_log/test_xmly_reverb.log
echo -e "\n"
