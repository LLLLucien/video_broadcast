# 发送端 broadcaster：采集 -> H.264 编码 -> 输出
# M2 阶段输出 mp4 文件；M3 阶段换成 RTP over UDP (端口 5004)
qt_add_executable(broadcaster
    src/main.cpp
    src/video_engine.cpp
)

# ---- 依赖查找 ----
# OpenCV：摄像头采集（M2 的采集源）
find_package(OpenCV REQUIRED)

# FFmpeg：编码(H.264) + 像素转换 + 封装
# Ubuntu 的 FFmpeg 没有 CMake config，统一走 pkg-config
# （已装 libavcodec-dev / libavformat-dev / libavutil-dev / libswscale-dev）
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
)

target_include_directories(broadcaster PRIVATE ${OpenCV_INCLUDE_DIRS})

target_link_libraries(broadcaster PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    vb_common
    ${OpenCV_LIBS}
    PkgConfig::FFMPEG
)
