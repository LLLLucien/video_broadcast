// 发送端（broadcaster）入口 —— 占位
// TODO: 摄像头采集 -> H.264 编码 -> RTP over UDP 发送到 5004 端口
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qInfo("broadcaster: placeholder (640x480@30fps H.264 -> RTP/UDP :5004)");
    return 0;
}
