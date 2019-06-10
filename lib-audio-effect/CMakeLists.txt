# CMake Options
cmake_minimum_required(VERSION 3.5.1)
if(POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()
if(POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

project(audio_effect C)

add_definitions(-D_GNU_SOURCE -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D_POSIX_C_SOURCE=200809L)

set(CMAKE_C_EXTENSIONS      ON)
set(CMAKE_C_STANDARD        99)
if(${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
elseif(${CMAKE_C_COMPILER_ID} MATCHES "^(GNU|.*Clang)$")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror=implicit -std=gnu99 -fPIC -s")

    if(${CMAKE_C_COMPILER_ID} STREQUAL "GNU" AND ${CMAKE_C_COMPILER_VERSION} VERSION_GREATER "6.0.0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wnarrowing -Werror=discarded-qualifiers -Werror=incompatible-pointer-types")
    endif()

    if(${CMAKE_C_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_C_COMPILER_ID} STREQUAL "AppleClang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wnarrowing -Werror=ignored-qualifiers -Werror=incompatible-pointer-types")
    endif()

    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fomit-frame-pointer")

    # message(STATUS "gcc/clang cflags ${CMAKE_C_FLAGS}")
    # message(STATUS "gcc/clang cflags release ${CMAKE_C_FLAGS_RELEASE}")
endif()

# set(CMAKE_CXX_EXTENSIONS    ON)
# set(CMAKE_CXX_STANDARD      11)
# if(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
# elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "^(GNU|.*Clang)$")
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror=implicit -std=gnu++11 -fno-exceptions -fno-rtti -fPIC -s")

#   if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER "6.0.0")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnarrowing -Werror=discarded-qualifiers -Werror=incompatible-pointer-types")
#   endif()

#   if(${CMAKE_CXX_COMPILER_ID} MATCHES ".*Clang$")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnarrowing -Werror=ignored-qualifiers -Werror=incompatible-pointer-types")
#   endif()

#   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fomit-frame-pointer")

#   # if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
#   #  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
#   # endif()

#   # message(STATUS "g++/clang++ cppflags ${CMAKE_CXX_FLAGS}")
#   # message(STATUS "g++/clang++ cppflags release ${CMAKE_CXX_FLAGS_RELEASE}")
#   # message(STATUS "exe linker flags ${CMAKE_EXE_LINKER_FLAGS}")
# endif()


include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src)

# ffmpeg
if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(FFmpeg_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/../prebuilt/android/prebuilt/ffmpeg-arm64/include)
    include_directories(${FFmpeg_INCLUDE_DIRS})
elseif(IOS)
    set(FFmpeg_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/../prebuilt/ios/)
    include_directories(${FFmpeg_INCLUDE_DIRS})
else()
    # include system lib
    include_directories("/usr/local/include")
    link_directories("/usr/local/lib")
endif()


add_library(effect
src/dsp_tools/fft/fft8g.c
src/dsp_tools/iir_design/iir_design.c
src/math/junior_func.c
src/noise_suppression/noise_estimation.c
src/pitch_tracker/blackman.c
src/pitch_tracker/low_pass.c
src/pitch_tracker/pitch_tracker.c
src/resample/resample.c
src/tools/avstring.c
src/tools/conversion.c
src/tools/dict.c
src/tools/fifo.c
src/tools/logger.c
src/tools/mem.c
src/tools/sdl_mutex.c
src/tools/util.c
src/voice_morph/morph_core.c
src/effects.c
src/echo.c
src/equalizer.c
src/minions.c
src/noise_suppression.c
src/voice_morph.c
src/xmly_audio_effects.c
src/xmly_echo.c
src/xmly_reverb.c
)


# set prefix as global var
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install")
# if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
#   set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "default install path" FORCE)
# endif()

install(DIRECTORY include DESTINATION .)

install(TARGETS effect
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    )

target_include_directories(effect
                           PRIVATE src
                           PUBLIC include)

if(ENABLE_TESTING AND NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Android" AND NOT IOS)
    add_executable(test_all_effects tests/test_all_effects.c)
    target_link_libraries(test_all_effects effect m  avutil swresample)

    add_executable(test_echo tests/test_echo.c)
    target_link_libraries(test_echo effect m pthread avutil swresample)

    add_executable(test_effect tests/test_effect.c)
    target_link_libraries(test_effect effect m pthread avutil swresample)

    add_executable(test_equalizer tests/test_equalizer.c)
    target_link_libraries(test_equalizer effect m pthread avutil swresample)

    add_executable(test_fifo tests/test_fifo.c)
    target_link_libraries(test_fifo effect m pthread avutil swresample)

    add_executable(test_logger tests/test_logger.c)
    target_link_libraries(test_logger effect m pthread avutil swresample)

    add_executable(test_minions tests/test_minions.c)
    target_link_libraries(test_minions effect m pthread avutil swresample)

    add_executable(test_noise_suppression tests/test_noise_suppression.c)
    target_link_libraries(test_noise_suppression effect m pthread avutil swresample)

    add_executable(test_voice_morph tests/test_voice_morph.c)
    target_link_libraries(test_voice_morph effect m  avutil swresample)

    add_executable(test_xmly_audio_effects tests/test_xmly_audio_effects.c)
    target_link_libraries(test_xmly_audio_effects effect m pthread avutil swresample)

    add_executable(test_xmly_echo tests/test_xmly_echo.c)
    target_link_libraries(test_xmly_echo effect m pthread avutil swresample)

    add_executable(test_xmly_reverb tests/test_xmly_reverb.c)
    target_link_libraries(test_xmly_reverb effect m pthread avutil swresample)
endif()