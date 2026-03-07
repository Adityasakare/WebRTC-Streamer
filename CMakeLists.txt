cmake_minimum_required(VERSION 3.16)
project(webrtc-streamer CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST     REQUIRED gstreamer-1.0)
pkg_check_modules(SOUP    REQUIRED libsoup-3.0)
pkg_check_modules(JSON    REQUIRED json-glib-1.0)
pkg_check_modules(GSTWEB  REQUIRED gstreamer-webrtc-1.0)
pkg_check_modules(GSTSDP  REQUIRED gstreamer-sdp-1.0)


add_executable(webrtc-streamer
    src/main.cpp
    src/Logger.cpp
)

target_compile_definitions(webrtc-streamer PRIVATE GST_USE_UNSTABLE_API)

target_include_directories(webrtc-streamer PRIVATE
    include
    ${GST_INCLUDE_DIRS}
    ${SOUP_INCLUDE_DIRS}
    ${JSON_INCLUDE_DIRS}
    ${GSTWEB_INCLUDE_DIRS}
    ${GSTSDP_INCLUDE_DIRS}
)

target_link_libraries(webrtc-streamer
    ${GST_LIBRARIES}
    ${SOUP_LIBRARIES}
    ${JSON_LIBRARIES}
    ${GSTWEB_LIBRARIES}
    ${GSTSDP_LIBRARIES}
)
