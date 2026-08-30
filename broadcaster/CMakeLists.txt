# 发送端 broadcaster：采集 -> H.264 编码 -> RTP over UDP (端口 5004)
qt_add_executable(broadcaster
    src/main.cpp
)

target_link_libraries(broadcaster PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    vb_common
)
