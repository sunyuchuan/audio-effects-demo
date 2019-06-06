#!/bin/bash

date_suffix=`date +%Y%m%d%H%M%S`
hash=`git rev-parse --short HEAD`
lib_name=audio-effect
release_package=${lib_name}-${date_suffix}-${hash}

cd ..
./cross_build.sh  
./package.sh  
cd -
cd AudioEffect/libeffect/src/main/jni/
ndk-build clean
ndk-build
cd -

tar zcvf $release_package.tar.gz AudioEffect/libeffect/src/main/java/ AudioEffect/libeffect/src/main/libs/ AudioEffect/libeffect/src/main/jni/effect-android-lib
# tar zcvf $release_package.tar.gz --exclude=libijkffmpeg-*.so AudioEffect/libeffect/src/main/java/ AudioEffect/libeffect/src/main/libs/ AudioEffect/libeffect/src/main/jni/effect-android-lib
