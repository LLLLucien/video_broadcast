// ============================================================
// main.cpp - 发送端（broadcaster）入口，命令行版（M2/M3）
// 用法：
//   ./broadcaster                             摄像头采集，写 out.mp4
//   ./broadcaster -s test                     无摄像头：测试画面写 out.mp4
//   ./broadcaster -s 0 -f rtp://127.0.0.1:5004   摄像头推流到本机（VLC 验证）
//   ./broadcaster -s 0 -f rtp://192.168.1.100:5004  摄像头推流到指定机器
// ============================================================
#include <QCoreApplication>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "video_engine.h"

// ------------------------------------------------------------------
// 打印命令行用法
// ------------------------------------------------------------------
static void print_usage(const char *prog)
{
    printf("用法: %s [-s 采集源] [-f 输出] [-t 秒数]\n", prog);
    printf("  -s 0        使用摄像头 /dev/video0（默认）\n");
    printf("  -s test     生成测试画面（无摄像头时验证编码链路）\n");
    printf("  -s 文件路径 使用视频文件作为采集源\n");
    printf("  -f out.mp4  录制到 mp4 文件（M2，默认 out.mp4）\n");
    printf("  -f rtp://IP:端口  推 RTP 网络流（M3），如 rtp://127.0.0.1:5004\n");
    printf("  -t 10       运行 10 秒后自动退出（默认 0 = 持续到按 q）\n");
    printf("示例:\n");
    printf("  %s -s test -f rtp://127.0.0.1:5004 -t 10   # 测试画面推流10秒\n", prog);
}

// ------------------------------------------------------------------
// 入口：解析命令行参数，创建引擎，跑主循环
// ------------------------------------------------------------------
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv); // Qt 骨架（后续阶段三套界面时沿用）

    // 默认参数
    std::string source = "0";       // 采集源：默认摄像头
    std::string output = "out.mp4"; // 输出文件
    int duration_sec = 0;           // 运行秒数：0 = 不限时

    // 手动解析命令行参数（保持直观，不引入额外库）
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
        {
            source = argv[++i];
        }
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
        {
            output = argv[++i];
        }
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc)
        {
            duration_sec = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-h") == 0)
        {
            print_usage(argv[0]);
            return 0;
        }
        else
        {
            printf("未知参数: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // 创建引擎并初始化（采集 + 编码器 + 输出文件）
    VideoEngine engine(source, output, duration_sec);
    if (!engine.init())
    {
        fprintf(stderr, "[broadcaster] 初始化失败，请检查采集源与输出路径\n");
        return 1;
    }

    // 主循环（阻塞，直到按 q / 到时 / 源结束）
    engine.run();
    return 0;
}
