#pragma once

#include <QByteArray>
#include <QImage>

#include <memory>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

// ---------------------------------------------------------------------------
// RAII 删除器：FFmpeg 的 C 资源不能用 delete，必须调用各自配对的释放函数。
// 每种结构体的释放方式不同，因此需要各自的专用删除器。
// 声明放在头文件里是为了让 std::unique_ptr 成员类型在类声明处可见可实例化；
// operator() 的实现放在 h264_decoder.cpp（那里才有 FFmpeg 头）。
// ---------------------------------------------------------------------------
struct AvCodecContextDeleter
{
    void operator()(AVCodecContext *p) const noexcept;
};

struct AvFrameDeleter
{
    void operator()(AVFrame *p) const noexcept;
};

struct AVPacketDeleter
{
    void operator()(AVPacket *p) const noexcept;
};

struct SwsContextDeleter
{
    void operator()(SwsContext *p) const noexcept;
};

using CodecContextPtr = std::unique_ptr<AVCodecContext, AvCodecContextDeleter>;
using FramePtr        = std::unique_ptr<AVFrame, AvFrameDeleter>;
using AVPacketPtr     = std::unique_ptr<AVPacket, AVPacketDeleter>;
using SwsContextPtr   = std::unique_ptr<SwsContext, SwsContextDeleter>;

class H264Decoder
{
public:
    H264Decoder() = default;
    ~H264Decoder();

    bool init();
    bool decodeNAL(const QByteArray &nal);
    const QImage &latestImage() const;

private:
    bool ensureRgbFrame(int width, int height);

    // 用 RAII 智能指针持有 FFmpeg 资源：所有权唯一、析构自动调用配对释放。
    CodecContextPtr codecContext_;
    FramePtr        frame_;
    FramePtr        rgbFrame_;
    SwsContextPtr   swsContext_;
    QImage latestImage_;
    int currentWidth_ = 0;
    int currentHeight_ = 0;
    int currentPixFmt_ = -1;
};
