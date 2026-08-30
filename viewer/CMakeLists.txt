# 接收端 viewer：RTP over UDP (端口 5004) -> H.264 解码 -> 显示
qt_add_executable(viewer
    src/main.cpp
)

target_link_libraries(viewer PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    vb_common
)
