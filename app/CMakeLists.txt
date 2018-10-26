cmake_minimum_required(VERSION 3.4.1)

add_library(
    player
    SHARED
    src/main/cpp/player.cpp
    src/main/cpp/queue.cpp
)

include_directories(src/main/cpp/include)

add_library(
    avcodec-lib
    SHARED
    IMPORTED
)
set_target_properties(
    avcodec-lib
    PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libavcodec.so
)

add_library(
    avfilter-lib
    SHARED
    IMPORTED
)
set_target_properties(
    avfilter-lib
    PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libavfilter.so
)

add_library(
    avformat-lib
    SHARED
    IMPORTED
)
set_target_properties(
    avformat-lib
    PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libavformat.so
)

add_library(
    avutil-lib
    SHARED
    IMPORTED
)
set_target_properties(
    avutil-lib
    PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libavutil.so
)

add_library(
    swresample-lib
    SHARED
    IMPORTED
)
set_target_properties(
    swresample-lib
    PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libswresample.so
)

add_library(
    swscale-lib
    SHARED
    IMPORTED
)
set_target_properties(
    swscale-lib
    PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libswscale.so
)

target_link_libraries(
    player
    log
    android
    avcodec-lib
    avfilter-lib
    avformat-lib
    avutil-lib
    swresample-lib
    swscale-lib
)