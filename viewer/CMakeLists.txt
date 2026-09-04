# 接收端 viewer：RTP over UDP (端口 5004) -> H.264 解码 -> 显示
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
)

add_executable(viewer
    src/main.cpp
    src/h264_decoder.cpp
    src/rtp_packet.cpp
    src/viewer_window.cpp
    src/video_receiver.cpp
)

target_link_libraries(viewer PRIVATE
    Qt5::Widgets
    Qt5::Network
    Qt5::Multimedia
    Qt5::MultimediaWidgets
    PkgConfig::FFMPEG
    vb_common
)
