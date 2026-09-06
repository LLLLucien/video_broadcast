# video_broadcast

基于 Linux、C++、Qt 的单向视频广播项目（电子教室：老师机广播，学生机同时收看）。
应用 OpenCV、FFmpeg、Qt 等音视频库。

- 发送端 broadcaster：OpenCV 采集(摄像头/测试画面/视频文件) → libswscale 转 YUV420P
  → libx264 软编码 H.264 → FFmpeg rtp muxer 推 RTP/UDP（支持单播与组播）
- 接收端 viewer：读取发送端生成的 SDP → FFmpeg rtp demuxer 收流 → H.264 解码 → Qt 显示

## 目录结构

```
common/      两端共享常量（端口/组播地址/分辨率/编码参数等，唯一参数源）
broadcaster/ 发送端（Qt GUI：一个"开始"按钮；也保留 -s/-f/-t CLI 无头模式）
viewer/      接收端（Qt GUI：QMainWindow）
build/       构建产物
```

## 依赖（两台机器都要装）

- 编译器与构建：`g++ cmake pkg-config`
- FFmpeg 开发库（两端都要）：
  `sudo apt install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev`
- OpenCV 开发库（只有发送端要，学生机不编 broadcaster 可跳过）：
  `sudo apt install libopencv-dev`
- Qt 5 或 Qt 6 任一（只需 Widgets 模块）：
  - Qt6 手动安装目录（如 ~/Qt/6.x/gcc_64），CMake 会自动搜索 ~/Qt
  - 或系统包：`sudo apt install qt6-base-dev`（Ubuntu 22.04+）/
    `sudo apt install qtbase5-dev`（Ubuntu 20.04）

## 构建

```bash
cd video_broadcast
cmake -S . -B build          # Qt 在非默认路径时加 -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build -j4
# 产物：build/broadcaster/broadcaster 与 build/viewer/viewer
```

## 单机自测（无摄像头也 OK）

```bash
# 终端 A：方式一：GUI 一键推流（无参数启动，点"开始"即推测试画面组播）
./build/broadcaster/broadcaster

# 终端 A：方式二：CLI 推流
./build/broadcaster/broadcaster -s test -f rtp://239.255.0.1:5004 -t 60

# 约 2 秒后生成 broadcast.sdp，再开终端 B 收流
./build/viewer/viewer broadcast.sdp
```

GUI 自测（自动开始、10 秒后自动停止）：`QT_QPA_PLATFORM=offscreen ./build/broadcaster/broadcaster --autostart -t 10`

无显示器（SSH/无头）验证：

```bash
QT_QPA_PLATFORM=offscreen ./build/viewer/viewer -t 10 broadcast.sdp
# 看到 "[viewer] 已解码 N 帧" 且 N>0 即链路正常
```

## 两台虚拟机组播联调（老师机 → 学生机）

**网络要求**：两台 VM 二层互通，且虚拟网络支持组播转发。

| 环境 | 网络模式 | 组播 |
|---|---|---|
| VMware | 仅主机(Host-only)/自定义 VMnet | 可通，推荐 |
| VMware | NAT | 同网段一般可通 |
| VirtualBox | NAT | 默认不通，改用仅主机/内部网络/Bridged |

先用 `ping` 确认两机互达。

**老师机**（一次操作，GUI / CLI 二选一）：

```bash
cd video_broadcast
# GUI：启动界面点"开始"（推流参数固定：测试画面 → 239.255.0.1:5004）
./build/broadcaster/broadcaster
# CLI：./build/broadcaster/broadcaster -s test -f rtp://239.255.0.1:5004 -t 600
# 运行约 2 秒后当前目录生成 broadcast.sdp（含组播地址与 H.264 参数集）
```

**学生机**（每台）：

```bash
# 1. 拷贝老师机生成的 broadcast.sdp 到本机（280 字节，一次配置，内容全校相同）
#    scp lmq@老师机IP:~/video_broadcast/broadcast.sdp ./

# 2. 收流
cd video_broadcast && ./build/viewer/viewer broadcast.sdp
```

**判定**：学生机 8 秒内出现老师机测试画面（渐变底 + 移动白块 + 帧号）即互通成功。

**注意**：老师机命令行里组播地址可改（如 239.255.0.1），学生机 SDP 必须与之一致；
组播属于 UDP，没有 ACK，学生机中途加入需等下一个关键帧（约 2 秒）才出画面，属正常。

## 常见问题

- **viewer 打开 SDP 失败 / Protocol 'rtp' not on whitelist**：属于旧版本二进制的报错，
  请重新编译（接收端已通过 `protocol_whitelist` 放行 rtp/udp）。
- **组播收不到画面**：两机 ping 通但 viewer 一直"等待信号"，多为虚拟网络不支持组播
  → 换 Host-only/Bridged 模式，或临时改用单播验证：
  ```bash
  # 老师机改推学生机 IP（SDP 会随之改变，需重新拷给学生机）
  ./build/broadcaster/broadcaster -s test -f rtp://学生机IP:5004 -t 600
  ```
- **防火墙拦 UDP 5004**：`sudo ufw allow 5004/udp`（或临时 `sudo ufw disable` 排查）。
- **Qt 版本报错**：学生机与老师机 Qt 大版本可不同（Qt5/Qt6 均可），
  不要在同一台机器同时用两套 Qt。

## 参考笔记

- ffmpeg 相关 API 使用：https://www.cnblogs.com/linuxAndMcu/category/1613476.html
- ffmpeg 实现推流视频：
  https://blog.csdn.net/ihungry/article/details/136742898




