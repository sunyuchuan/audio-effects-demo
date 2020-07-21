

Linux  :
    cd lib-audio-effects
    ./run_test.sh

Android:
    Android Need [NDK r14b](https://dl.google.com/android/repository/android-ndk-r14b-linux-x86_64.zip)
    ./cross_build.sh
    ./package.sh
    cd android/audio-effects-armv7a/src/main/jni/
    ndk-build
    cd -
    cd android/audio-effects-armv5/src/main/jni/
    ndk-build
    cd android/audio-effects-arm64/src/main/jni/
    ndk-build
    cd android/audio-effects-x86/src/main/jni/
    ndk-build
    cd android/audio-effects-x86_64/src/main/jni/
    ndk-build
    install the apk to android,now you can use the score on android

iOS   :
    iOS Need Xcode
    ./cross_build.sh
    ./package.sh
    need ffmpeg,the ffmpeg lib in IJKMediaFramework. so you need import the IJKMediaFramework to project
