#!/bin/bash
rm -rf out
./cross_build.sh clean
./cross_build.sh
./package.sh
cd android/AudioEffect/libeffect/src/main/jni/
ndk-build clean
ndk-build 