cmake_minimum_required(VERSION 3.16)
project(video_broadcast VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6.11.1（~/Qt 安装目录）
list(PREPEND CMAKE_PREFIX_PATH "/home/lmq20233547/Qt/6.11.1/gcc_64")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 视频广播用到的 Qt 模块
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia MultimediaWidgets)

add_subdirectory(common)
add_subdirectory(broadcaster)
add_subdirectory(viewer)
