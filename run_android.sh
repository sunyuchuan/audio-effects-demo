#!/bin/bash

./cross_build.sh  
./package.sh  
cd android/AudioEffect/libeffect/src/main/jni/
ndk-build 