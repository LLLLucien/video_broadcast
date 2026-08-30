// 接收端（viewer）入口 —— 占位
// TODO: RTP over UDP 接收 5004 端口 -> H.264 解码 -> 窗口显示
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qInfo("viewer: placeholder (RTP/UDP :5004 -> H.264 decode -> display)");
    return 0;
}
