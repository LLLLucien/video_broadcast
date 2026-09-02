# 项目上下文文档：单向视频广播系统（video_broadcast）

> **本文档用途**：供虚拟机开发环境中的 AI 助手快速了解项目全貌，高效协助开发。
> **信息优先级**：以本文档与《单向视频广播系统需求规格说明书-草案V0.4(1).docx》为准（V0.4(1) 为最新需求基线，在 V0.4 基础上补充了接口契约、错误码、性能量化与联调清单）。更早的 V0.1~V0.4 文档仅作演进参考，**不要**以其为准。
> **最后更新**：2026-09-02（新增：M0~M3 实测完成状态、摄像头透传完成、本次 VLC 联调验证记录，见 §5 与 §7；旧文档中"开发尚未开始/M0 待做/main.cpp 占位"等描述均已过期）

---

## 1. 项目概述

### 1.1 一句话定位

基于 Linux + C++ + Qt 的**单向视频广播系统**：一个发送端采集摄像头画面，编码后经局域网广播，多个接收端同时收看。课程双人练手项目（简历级），非生产项目。

### 1.2 项目目标

- 完整跑通「采集 → 编码 → 网络传输 → 解码 → 显示」整条视频链路
- 发送端与接收端各自带一个 Qt 图形界面（一人一端，双方都有可讲的简历亮点）
- 局域网内：1 个发送端 → 至少 3 台接收端同时稳定收看
- 端到端延迟 ≤300ms（软件编码场景）
- 断流后 2 秒内自动恢复画面（短 GOP 保证）

### 1.3 核心功能

| 端 | 核心功能 |
|---|---|
| 发送端 Broadcaster | OpenCV 采集 → BGR 转 YUV420P → libx264 编码 H.264 → RTP 封装 → UDP 发送；Qt 界面（开始/停止、参数配置、预览、状态栏） |
| 接收端 Viewer | UDP 收包 → RTP 解包重组 → FFmpeg 解码 → YUV420P 转 BGR → imshow/Qt 显示；断流检测与自愈 |
| 公共 | 两端日志（控制台）、组播多接收端、联调验证工具（VLC 充当标准接收端） |

### 1.4 整体架构（数据的一生）

```
摄像头
  │  OpenCV VideoCapture 采集（BGR 帧，640×480/30fps，约 0.9MB/帧）
  ▼
格式转换（FFmpeg libswscale：BGR → YUV420P）
  ▼
编码（FFmpeg libx264 软编：YUV420P → H.264 码流，约 1~4Mbps）
  ▼
RTP 封装 + UDP 发送（libavformat + 原生 socket；阶段一单播，阶段三组播 239.255.0.1:5004）
  ▼  局域网传输（H.264 码流 ≈ 2Mbps，百兆网轻松承载）
接收重组（按 RTP 序号拼回完整 H.264 帧）
  ▼
解码（FFmpeg：H.264 → YUV420P）
  ▼
格式转换（libswscale：YUV420P → BGR）
  ▼
显示（阶段一/二 imshow，阶段三 Qt 窗口）
```

**为什么必须编码**：原始 BGR 帧 640×480@30fps ≈ 216Mbps，远超带宽且超出单个 UDP 包上限（约 64KB）；H.264 压缩后降到 1~4Mbps，且必须拆包发送、接收端按序号重组。

### 1.5 当前进度

- ✅ 需求文档：SRS V0.4(1) 完成（含接口契约、错误码表、联调清单）
- ✅ 仓库：GitHub `LLLLucien/video_broadcast`（Public）+ Gitee 镜像 `Lucien_Memory/video_broadcast`，均走 SSH
- ✅ 仓库骨架初始化**已完成**：目录结构 broadcaster/viewer/common 齐全；.gitignore 已含视频忽略（*.mp4/*.avi/*.h264/*.264）；根 CMakeLists 配置 Qt 6.11.1；build/ 可 configure
- ✅ 分支已建：`main`（稳定主线）、`lmq-dev`（本人）、`yht-dev`（队友）；本地当前在 `lmq-dev`
- ✅ **发送端开发 M1~M3 已完成并实测通过（2026-09-02，详见 §7）**：
  - M1 采集循环：OpenCV 采集，支持 摄像头(/dev/video0) / 测试图(test) / 视频文件 三种源
  - M2 编码到 mp4：libx264 软编写文件，ffplay/ffprobe 验证时长与帧率正确
  - M3 发送到网络：FFmpeg rtp muxer 推 UDP/RTP 并生成 broadcast.sdp；ffplay 与 VLC（snap 版）均验证可播
- ⬜ M4 自研接收端：队友（yht）职责

**开发路线（三阶段，已确认）**：
1. **阶段一**：发送端引擎（OpenCV 采集 + FFmpeg 编码 + UDP 发送）+ **VLC 当接收端**验证流能播 —— 跑通即证明发送端正确
2. **阶段二**：自研接收端（socket 收包 → FFmpeg 解码 → imshow 显示）
3. **阶段三**：两端套 Qt 界面 + 切换 UDP 组播支持多接收端
4. 之后才考虑：音频、录像、桌面直播（Wayland/PipeWire）等扩展

---

## 2. 技术背景

### 2.1 技术栈与职责

| 环节 | 技术 | 掌握程度 | 说明 |
|---|---|---|---|
| 摄像头采集 | OpenCV 4.x `VideoCapture` | ✅ 已学过 | 底层 V4L2 已被 OpenCV 封装，**不直接调 V4L2** |
| 像素格式转换 | FFmpeg `libswscale`（sws_scale 一个函数） | 🆕 新学 | BGR→YUV420P / YUV420P→BGR，两端共用 |
| 编码 | FFmpeg `libavcodec` + libx264（**软编**） | 🆕 新学 | 本项目最大新知识块；**不做 VAAPI 硬编** |
| RTP 封装/解封装 | FFmpeg `libavformat` | 🆕 半新 | 发送端调现成 muxer；接收端可借助 FFmpeg 接口 |
| 网络传输 | 原生 POSIX socket（sendto/recvfrom） | ✅ 已熟 | 与用户之前聊天服务器项目同一套 API |
| 图形界面 | Qt 5/6 Widgets + 信号槽 | 📖 正课内容 | 两端各一个界面 |
| 验证工具 | VLC | — | 当"标准接收端"验证发送端正确性 |
| 已明确排除 | Boost、OpenGL、V4L2 直调、VAAPI、ALSA、H.265、MySQL/SQLite、公网穿透、连麦回传 | — | 范围外，防蔓延 |

### 2.2 运行环境与依赖

**已实测确认（2026-08-31，本地虚拟机）：**
- 操作系统：Ubuntu 24.04.4 LTS（x86_64）
- 已装依赖：OpenCV **4.6.0**、FFmpeg **6.1.1**（libavcodec 60.31.102）、libavformat/libavutil/libswscale 同源、Qt **6.11.1**（手动装于 `~/Qt/6.11.1/gcc_64`，根 CMakeLists.txt 已配置）
- 摄像头透传**已完成**（2026-09-02 实测）：`/dev/video0` 可用，OpenCV 以 GStreamer 后端打开成功；`-s test` 测试图模式保留作为无摄像头兜底
- build/ 已成功 configure 一次，CMake/Qt 找包链路正常，可增量编译
- 硬件：带摄像头的 PC；发送端建议 4 核以上（软编码吃 CPU）
- 网络：局域网；组播跨网段需 IGMP Snooping（实验室同网段一般无问题）

依赖安装清单（M0 阶段，重装新环境时用）：

```bash
sudo apt install build-essential pkg-config cmake git \
    libopencv-dev ffmpeg \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
```

- 验证三连：`pkg-config --modversion opencv4`、`ffmpeg -version`（建议 6.x）、`ls /dev/video0`
- 终极验证：`ffplay /dev/video0` 能看到自己画面

### 2.3 目录结构（GitHub 仓库布局）

```
video_broadcast/                  ← GitHub: LLLLucien/video_broadcast（Gitee 镜像: Lucien_Memory）
├── README.md                     ← 项目简介（含"新分支合并测试"等测试字样，勿删）
├── docs/                         ← lmq的上下文总结.md（本文档）
├── .gitignore                    ← GitHub C++ 模板 + 视频忽略（*.mp4/*.avi/*.h264/*.264）
├── common/                       ← 两端共享：CMakeLists(vb_common INTERFACE 库) + params.h（参数契约占位）
├── broadcaster/                  ← 发送端（**lmq 地盘，只在这里写代码**）
│   ├── CMakeLists.txt            ← qt_add_executable，链接 Qt6 Widgets/Network/Multimedia + vb_common
│   └── src/main.cpp              ← 占位（QCoreApplication 打印一行日志），待实现
└── viewer/                       ← 接收端（队友 yht 地盘，lmq 不动）
    ├── CMakeLists.txt
    └── src/main.cpp              ← 占位
build/（本地存在，已 gitignore）    ← 已 configure 过，勿提交
```

**地盘边界**：`broadcaster/` 与 `viewer/` 是双方私有目录，各自只提交自己的；`common/` 与 `docs/` 改动前必须通知对方。

### 2.4 关键模块职责

- `common/params.h`：两端参数契约（分辨率 640×480、帧率 30、H.264、RTP over UDP、端口 5004）——**联调对不上的根源往往就是这里没对齐，改动需双方确认**
- `Broadcaster` 引擎类：采集→转换→编码→发送流水线，**纯 C++ 不依赖界面**，提供 start/stop 接口；命令行版本永久保留作为调试后门
- `Viewer` 引擎类：收包→重组→解码→显示流水线，同样引擎/界面分离
- Qt 界面层：只调用引擎接口 + 信号槽更新界面

---

## 3. 开发规范

### 3.1 代码风格（用户明确偏好）

- **直观简单、可读性优先**；禁止浓缩/取巧的单行写法；显式逻辑 + 逐行中文注释
- 非头文件函数加 Doxygen 注释：`/** @brief ... @param ... @retval ... */`
- 函数内部注释用 `//`，不用 `/* */`
- 头文件末尾 `#endif` 带注释：`#endif /* __XXX_H__ */`（宏用头文件大写名）
- 宏定义/常量约定放源文件或 common 中明确位置，**不默认塞头文件**
- 偏好以传入数组/参数的方式编写函数（不用全局状态），简单直白

### 3.2 命名规则

- 类型/类：大驼峰（`Broadcaster`、`VideoEngine`）
- 函数/变量：小驼峰或 snake_case 均可，但**全项目统一**（建议 snake_case 与 C 代码习惯一致）
- 常量/宏：全大写 + 下划线（`DEFAULT_PORT`）
- 文件：snake_case（`m1_capture.cpp`、`params.h`）

### 3.3 目录组织方式

- 引擎与界面分层：`engine/` 与 `ui/` 分开（或 src 内按模块分文件），界面不得包含采集/编码逻辑
- 两端各自独立 CMakeLists.txt，根目录暂不强制统一构建（可后续加）
- 中间产物（build/、录像文件）一律不提交

### 3.4 分支与提交规范

- **main**：永远是能编译、能演示的版本（进度展示就靠它）
- **lmq-dev（本人）/ yht-dev（队友）**：各自日常开发分支，稳定后合并回 main（分支名已实际如此，旧文档中的 dev_broadcaster/dev_viewer 作废）
- **合并方式（已实际采用）**：GitHub Pull Request（yht-dev 已通过 PR #1 合入 main；lmq-dev 也有 PR 合并记录）；面对面联调时可临时本地 merge，主干合入以 PR 为主
- 联调关键节点打 tag：如 `git tag v0.1-vlc-ok`，演示前出问题可回退
- 提交信息：简洁中文或英文，说明做了什么（如 `init: project skeleton with broadcaster/viewer/common dirs`）
- `.gitignore` 必须包含：`build/`、`*.o`、`CMakeCache.txt`、`CMakeFiles/`、**`*.mp4` / `*.avi` / `*.h264` / `*.264`**（测试录像进仓库会把仓库撑爆；**已确认当前仓库均已包含**）

### 3.5 个人职责边界（lmq 专属，务必遵守）

- **lmq 负责发送端（broadcaster）**：代码只写在 `broadcaster/` 目录内（含头文件），**不得改动/污染其他目录**
- `viewer/` 是队友（yht）的地盘，非对接需要不动；`common/`（params.h 参数契约）与 `docs/` 的改动需先与队友确认
- 仓库中出现队友代码、`build/`、其他文档均属正常，**不要**在 lmq-dev 分支上替队友写代码或提交其目录下的文件
- 提交前先 `git status` 确认改动范围，只 `git add` 自己 `broadcaster/`（及经确认的公共文件）下的内容，避免误提交他人文件
- 只有在「联调对接」阶段才可能涉及代码整合（改 common/params.h、对齐接口契约），届时先与队友沟通再动

---

## 4. 关键决策记录

### 4.1 已确定的技术选型与原因

| # | 决策 | 原因 |
|---|---|---|
| 1 | 采集用 OpenCV VideoCapture，而非直调 V4L2 | 用户已学过 OpenCV；V4L2 已被封装，少学一个栈 |
| 2 | 编码用 libx264 **软编**，不做 VAAPI 硬编 | 简单可控、学习价值高；硬编可后期作为优化项 |
| 3 | 传输用 RTP over UDP，阶段一单播、阶段三组播 | 单向广播天然适合 UDP；组播实现一对多且发送负载不随接收端数量增长 |
| 4 | 用**短 GOP（≤2 秒，约 60 帧一个 I 帧）**替代 I 帧请求机制 | 接收端丢包后最多等 2 秒自动恢复，**省掉 TCP 信令通道**，首期工作量大幅降低 |
| 5 | VLC 当标准接收端验证发送端 | 把"发送端对不对"和"接收端对不对"两个问题解耦，可并行开发 |
| 6 | 引擎与界面严格分离，命令行版本永久保留 | 先命令行跑通再套 Qt，排查范围小；命令行版是界面版的调试后门 |
| 7 | 一人一端分工（A 发送端 / B 接收端） | 系统天然拆两个程序；知识侧重错开、互不卡进度；双方简历各有亮点 |
| 8 | 接口契约先行：PT=96、90000Hz、FU-A 分包、239.255.0.1:5004 | 联调问题九成出在"约定不一致"，开工前定死 |
| 9 | 端到端延迟指标定为 ≤300ms | 软编码场景比 200ms 更现实 |
| 10 | 明确排除范围：Boost/OpenGL/VAAPI/H.265/数据库/连麦 | 防范围蔓延，练手项目先跑通主线 |

### 4.2 已知限制与待规避的坑

- **sws_scale 三大坑**（格式转换最容易翻车）：
  1. **stride ≠ 宽度**：`linesize` 可能大于宽（内存对齐补字节），读像素必须用 linesize 走行，否则画面斜着撕裂
  2. **U/V 顺序**：YUV420P 是 U 在前 V 在后；接反不报错但人脸发蓝、背景发紫
  3. **宽高必须偶数**：420 是 2×2 共享色度；640×480 没问题，非偶数尺寸需先 resize
- **Qt 铁律**：界面控件只能主线程操作；采集/编码/发送循环必须放工作线程，更新界面走**信号槽**，工作线程不得直接调控件
- **预览窗口**：Qt 界面里显示摄像头画面 = 工作线程把 `Mat` 转 `QImage` 塞 QLabel（重写 paintEvent），**不是 imshow**（imshow 只在纯 OpenCV 程序用）
- **视频文件绝不入库**：测试录像 .mp4 等必须被 .gitignore 排除
- **摄像头透传**：虚拟机默认看不到摄像头，需 VMware「虚拟机→可移动设备→连接」/ VirtualBox「设备→USB」透传后才有 `/dev/video0`
- **采集帧率低**：实际帧率只有 10~15fps 时先排查 `cap.set(cv::CAP_PROP_FOURCC, MJPG)` 类设置，低帧率会搅浑后续所有环节
- **联调常见问题速查**：无画面→组播/防火墙（tcpdump 抓包）；花屏/绿屏→丢包/GOP 过大（检查 I 帧间隔）；延迟大→Jitter Buffer 缓冲太大；卡顿→编码线程 CPU 不足（top 查看）
- **错误码约定**：1001 摄像头打开失败（提示检查设备）、1002 编码器初始化失败（降级软编）、2001 组播加入失败（提示检查网络）、2002 断流超时（自动重连）

---

## 5. 当前状态

### 5.1 正在进行的任务

| 任务 | 状态 | 说明 |
|---|---|---|
| SRS 需求文档 | ✅ 完成 | V0.4(1) 为最新基线 |
| GitHub 仓库创建 | ✅ 完成 | LLLLucien/video_broadcast，Public |
| 仓库骨架初始化（虚拟机） | ✅ 完成 | 目录骨架、.gitignore（含视频忽略）、common/params.h 均已提交；build/ 可 configure |
| M0 环境搭建 | ✅ 完成 | 依赖装齐（OpenCV 4.6.0/FFmpeg 6.1.1/Qt 6.11.1）；摄像头透传完成（/dev/video0 可用） |
| M1 采集循环 | ✅ 完成 | OpenCV 采集 + 测试图/视频文件多源支持，在 VideoEngine::init_capture/read_frame |
| M2 编码到文件 | ✅ 完成 | libx264 软编 + 写 mp4，ffplay/ffprobe 验证通过（时长/帧率正确） |
| M3 发送到网络 | ✅ 完成 | rtp muxer 推 UDP 单播并生成 broadcast.sdp；ffplay 与 VLC（snap 版）验证可播（2026-09-02） |
| M4 自研接收端 | ⬜ 待做 | recvfrom → 解码 → imshow（队友职责）；发送端已就绪可联调 |

### 5.2 待解决问题与优先级

| 优先级 | 问题 | 说明 |
|---|---|---|
| 中 | 音频是否纳入 | 建议有余力再做（涉及音视频同步），不承诺 |
| 中 | 验收演示接收端数量 | 视实验室机器数量（目标 ≥3 台） |
| 低 | 界面完成度要求 | 够用即可 or 作为加分项打磨 |
| 低 | 桌面直播源（Wayland/PipeWire） | P2/P3 远期扩展，不影响主线 |

### 5.3 虚拟机环境特殊注意事项

**已实测确认（2026-08-31）：**
- 系统：Ubuntu 24.04.4 LTS（x86_64）；用户 `lmq20233547`；VSCode Remote-SSH；shell 为 **fish**（通配符行为与 bash 不同：如 `ls /dev/video*` 无匹配会直接报错，可改用 `ls /dev | grep video`）
- 依赖：OpenCV 4.6.0、FFmpeg 6.1.1（libavcodec 60.31.102）、Qt 6.11.1（`~/Qt/6.11.1/gcc_64`，根 CMakeLists 已配置）——均可用
- 摄像头透传**已完成**（/dev/video0 可用，2026-09-02 实测）；透传方式：VMware「虚拟机→可移动设备→连接」/ VirtualBox「设备→USB」
- build/ 已成功 configure 过一次，CMake + Qt 找包链路正常，可增量编译
- Git：本地在 `lmq-dev`（跟踪 `origin/lmq-dev`）；远程 `main`/`lmq-dev`/`yht-dev`；GitHub（origin）+ Gitee 双远程，默认推送 GitHub，Gitee 手动 `git push gitee ...` 镜像
- 代码现状：broadcaster 已完成 M2/M3（`src/` 下 main.cpp + video_engine.h/.cpp，纯命令行引擎）；viewer 的 src/main.cpp 仍为占位
- **VLC 版本陷阱**：系统版 `/usr/bin/vlc`（3.0.20-3build6）对 ffmpeg 6.x 生成的 SDP 解析失败（黑屏）；**必须用 snap 版 `/snap/bin/vlc`**（3.0.20-1-g2617de71b6）才能播 broadcast.sdp，详见 §7

- **权限边界**：`sudo` 安装、GitHub 认证、网络类操作由用户手动执行；AI 遇到权限阻塞应**停下列出要用户手动执行的命令**，不要死循环重试，不要擅自改用危险操作
- Git 优先 **SSH 方式**（用户之前 HTTPS+token 吃过亏）；GitHub 网络不稳时可用 Gitee 镜像保底
- 用户不在虚拟机直接对话，代码由 AI 协助编写；**AI 应主动说明每步在做什么、给出可验证的产出标准**
- 摄像头透传失败则一切采集工作无从谈起
- 两端联调时注意 FFmpeg 版本一致（本机 6.1.1，符合建议 6.x）

---

## 6. 协作期望

### 6.1 期望 AI 协助的具体场景

1. **代码编写**：FFmpeg C API 骨架（M2 六函数）、Qt 界面（信号槽、QImage 显示）、socket 收发包
2. **调试**：花屏/绿屏/撕裂（优先查 sws_scale 三坑）、延迟、断流恢复、CPU 占用
3. **重构**：引擎/界面分层、代码风格统一（Doxygen 注释、命名）
4. **答疑**：YUV 概念、RTP 打包细节、Qt 线程模型、CMake 配置等，解释要通俗
5. **联调支持**：对照接口契约（PT/端口/GOP）检查两端参数一致性

### 6.2 回复格式偏好

- **中文回复**，结构化输出（标题层级 + 代码示例 + 列表 + 对比表格）
- 先给**结论/原因**，再给具体代码；定位**根本原因**并给出**具体修改点**，不要整段重写
- 代码要简单直白、带中文注释；解释概念时先讲底层机制再讲抽象
- 用户会先提出自己的理解来确认——**先确认他的理解对不对，再补充**
- 分步增量指导，不要一次抛太多新概念；每个节点给出"能看到什么结果"的验证标准
- 用户不熟悉 FFmpeg/Qt 的具体 C API 细节，但理解"调用 API 就能完成"的层面；解释时给 API 名 + 一句话用途 + 示例即可，不必展开内部实现

### 6.3 用户背景速览（帮助调整解释深度）

- 中南林业科技大学计算机科学专业准大四（秋招求职中），方向嵌入式 C/C++
- 本项目分工：**lmq 负责发送端（broadcaster）**，代码只在 `broadcaster/` 内写
- 已掌握：C socket 编程（TCP/UDP）、pthread 多线程与同步原语、OpenCV 基础（打开摄像头/帧处理/VideoWriter）、Linux 系统编程
- 本项目是他 C++/Qt 方向的简历级实践项目，**面试要能讲清楚每一环**
- 学习风格：先理解底层再使用抽象、喜欢逐行注释的示例代码、追求通用方案而非临时凑合

---

## 7. 联调验证记录（2026-09-02，lmq 发送端 M2/M3 实测）

### 7.1 本次验证了什么（全部通过）

| 场景 | 命令 | 结果 |
|---|---|---|
| M2：测试图编码写 mp4 | `./broadcaster -s test -t 3` → 生成 out.mp4 | ✅ ffprobe/ffplay 可播，时长≈3s、30fps 正确 |
| M3：测试图推 RTP | `./broadcaster -s test -f rtp://127.0.0.1:5004 -t 60` | ✅ 收端可播 |
| M3：摄像头推 RTP | `./broadcaster -s 0 -f rtp://127.0.0.1:5004 -t 60` | ✅ /dev/video0 打开成功，持续推流 |
| 接收：ffplay | `ffplay -protocol_whitelist file,rtp,udp broadcast.sdp` | ✅ 出画面 |
| 接收：VLC | `/snap/bin/vlc broadcast.sdp` | ✅ 出画面（阶段一验收点达成） |

运行产物：`broadcast.sdp`（发送端在运行目录自动生成，接收端说明书）；调试遗留 `vlc.sdp`（已无用，勿提交）。

### 7.2 遇到的问题与解决（逐条记录，含 VLC 黑屏全程）

1. **C++ 链接 FFmpeg 符号错误**（符号被 C++ mangle 成 `_Z13avcodec_open2...`）
   - 原因：该环境 FFmpeg 头文件不自带 `extern "C"` 保护
   - 解决：`#include` 外包一层 `extern "C" { ... }`
2. **`free(): invalid pointer` 崩溃**
   - 原因：栈上裸 `AVPacket` 传给 `avcodec_receive_packet()`，其内部先 unref 野指针
   - 解决：必须 `av_packet_alloc()` 分配（FFmpeg 5.x+ 要求），写包与 flush 两处都改
3. **mp4 时长错误**（duration≈0.0058s、r_frame_rate=15360）
   - 原因：pts/dts 未从编码器基准(1/30)换算到 muxer 实际基准（mp4 会改 time_base）
   - 解决：写包与 flush 处均 `av_rescale_q_rnd` 换算到 `out_stream->time_base`
4. **ffplay 报 `Protocol 'rtp' not on whitelist 'file,crypto,data'`**
   - 原因：Ubuntu ffmpeg 默认协议白名单不含 rtp/udp（安全加固）
   - 解决：加参数 `-protocol_whitelist file,rtp,udp`（VLC 无此限制）
5. **VLC 打开 broadcast.sdp 黑屏（排查全过程）**
   - 现象①：系统版 VLC 3.0.20-3build6 填 `rtp://` 地址播 → 必然黑屏（裸 RTP H.264 无 SDP 参数/SPS-PPS，解不了）→ 正确做法是**打开 sdp 文件**
   - 现象②：打开 sdp 仍黑屏，`cvlc --verbose 2` 日志见 `sap demux: unexpected SDP line: 0x62 / invalid SDP`，随后兜底选错 `ps demuxer` → garbage
     - 原因：VLC 3.0.x 的 SDP 解析器(sap demux)脆弱，遇 `b=AS:2000` 行直接判无效（0x62='b'）；fmtp 行内分号后空格也不耐受
     - 尝试序列（均无效）：删 `b=AS:` 行 → 仍黑屏；强制 `--demux=rtp` → 报"无法打开 MRL"；手工重排 sdp（去 fmtp 空格曾误删 `a=fmtp:96 ` 必需空格，修正后补 `a=rtcp:5005`、删 `a=tool:`）→ 仍黑屏
   - 深层原因：发送端设 `AV_CODEC_FLAG_GLOBAL_HEADER` → SPS/PPS 只在 SDP 的 sprop-parameter-sets 里、流内不带；VLC 一旦 SDP 解析环节失败就拿不到 SPS/PPS，永远解不出画面
   - 升级尝试：`sudo snap install vlc` 装出仍是 **3.0.20-1-g2617de71b6**（VideoLAN 尚无 4.x 稳定版，snap 稳定频道只有 3.0.x）
   - 最终解法：**改用 snap 版 VLC** `/snap/bin/vlc`（3.0.20-1-g2617de71b6），直接打开原始 broadcast.sdp 即出画面（无需任何手工改 sdp）
   - 结论：**VLC 版本差异是主要变量**（系统版 3.0.20-3build6 的 SDP 解析有 bug）；本机验证一律用 `/snap/bin/vlc`
6. **VLC 出画面后日志有 `main decoder error: buffer deadlock prevented`**
   - 非致命（启动缓冲/解码器短暂忙的自救提示），画面流畅可忽略；若卡顿加 `--network-caching=300 --rtp-caching=200`
7. **UDP 是"无连接"协议**（发送端丢包、接收端 bind 收）
   - 同机验证：收发都用 `127.0.0.1` 即可，不必改虚拟网卡 IP
   - 接收端程序建议 bind `0.0.0.0:端口`（不挑发送端 IP，跨机联调也不用改代码）

### 7.3 发送端当前技术参数（接收端/联调需对齐，来源 broadcaster/src/video_engine.cpp）

- 分辨率 **640×480**、帧率 **30fps**、像素 YUV420P
- 编码 **H.264（libx264 软编，preset=ultrafast）**、码率 **2Mbps**、**GOP=60 帧（约 2 秒 1 个 I 帧）**、**无 B 帧**
- RTP 契约：**payload type 96、90000Hz**、单播端口 **5004**（阶段一/二；阶段三组播 239.255.0.1）
- ⚠️ **SPS/PPS 关键点**：编码器设了 `AV_CODEC_FLAG_GLOBAL_HEADER` → **RTP 流内不带 SPS/PPS**，只在 SDP 的 `sprop-parameter-sets` 里
  - 接收端若用 FFmpeg 的 sdp demuxer 打开 broadcast.sdp → 自动配置解码器，无感
  - 接收端若自研裸收 RTP → 必须自行解析 SDP 的 sprop-parameter-sets 喂给解码器（或等带内 I 帧配置），否则永远无法开始解码（黑屏）
- RTP 分包（FU-A 等）由 FFmpeg rtp muxer 自动完成，发送端不手写 socket
