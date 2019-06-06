#!/bin/bash

cd ..
./cross_build.sh  
./package.sh  
cd -
cd AudioEffect/libeffect/src/main/jni/
ndk-build clean
ndk-build
cd -

rm -rf audio-effect.tar.gz
tar zcvf audio-effect.tar.gz AudioEffect/libeffect/src/main/java/ AudioEffect/libeffect/src/main/libs/ AudioEffect/libeffect/src/main/jni/effect-android-lib
# tar zcvf audio-effect.tar.gz --exclude=libijkffmpeg-*.so AudioEffect/libeffect/src/main/java/ AudioEffect/libeffect/src/main/libs/ AudioEffect/libeffect/src/main/jni/effect-android-lib
