#pragma once

#include <QByteArray>
#include <QImage>

struct AVCodecContext;
struct AVFrame;
struct SwsContext;

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

    AVCodecContext *codecContext_ = nullptr;
    AVFrame *frame_ = nullptr;
    AVFrame *rgbFrame_ = nullptr;
    SwsContext *swsContext_ = nullptr;
    QImage latestImage_;
    int currentWidth_ = 0;
    int currentHeight_ = 0;
    int currentPixFmt_ = -1;
};

