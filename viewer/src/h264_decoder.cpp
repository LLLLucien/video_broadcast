#include "h264_decoder.h"

#include <iostream>
#include <cstring>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libswscale/swscale.h>
}

H264Decoder::~H264Decoder()
{
    if (frame_)
    {
        av_frame_free(&frame_);
    }
    if (rgbFrame_)
    {
        av_freep(&rgbFrame_->data[0]);
        av_frame_free(&rgbFrame_);
    }
    if (codecContext_)
    {
        avcodec_free_context(&codecContext_);
    }
    if (swsContext_)
    {
        sws_freeContext(swsContext_);
    }
}

bool H264Decoder::init()
{
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        std::cerr << "H264 decoder not found" << std::endl;
        return false;
    }

    codecContext_ = avcodec_alloc_context3(codec);
    if (!codecContext_ || avcodec_open2(codecContext_, codec, nullptr) < 0)
    {
        std::cerr << "Failed to initialize H264 decoder" << std::endl;
        return false;
    }
    codecContext_->thread_count = 0;
    codecContext_->thread_type = FF_THREAD_SLICE;
    codecContext_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codecContext_->flags2 |= AV_CODEC_FLAG2_FAST;

    frame_ = av_frame_alloc();
    rgbFrame_ = av_frame_alloc();
    if (!frame_ || !rgbFrame_)
    {
        std::cerr << "Failed to allocate H264 decoder resources" << std::endl;
        return false;
    }

    std::cout << "H264 decoder initialized successfully" << std::endl;
    return true;
}

bool H264Decoder::decodeNAL(const QByteArray &nal)
{
    if (!codecContext_ || nal.isEmpty())
    {
        return false;
    }

    // nal is already an access unit whose NALs are separated by start codes
    // (0x00000001). Feed it to the decoder as a single packet to avoid the
    // redundant secondary parser pass. A one-time packet allocation per frame
    // replaces the former per-fragment allocations inside the parse loop.
    AVPacket *packet = av_packet_alloc();
    if (!packet)
    {
        return false;
    }
    // nal stays alive during avcodec_send_packet(); the decoder only references
    // the buffer during this call, so const_cast away of the read-only data is safe.
    packet->data = const_cast<uint8_t *>(
        reinterpret_cast<const uint8_t *>(nal.constData()));
    packet->size = nal.size();
    bool decoded = false;
    if (avcodec_send_packet(codecContext_, packet) >= 0)
    {
        while (true)
        {
            const int ret = avcodec_receive_frame(codecContext_, frame_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret < 0)
            {
                break;
            }

            if (!ensureRgbFrame(frame_->width, frame_->height))
            {
                av_frame_unref(frame_);
                break;
            }

            sws_scale(swsContext_, frame_->data, frame_->linesize, 0, frame_->height,
                      rgbFrame_->data, rgbFrame_->linesize);
            latestImage_ = QImage(reinterpret_cast<uchar *>(rgbFrame_->data[0]),
                                  frame_->width, frame_->height,
                                  rgbFrame_->linesize[0], QImage::Format_RGB888).copy();
            decoded = true;
            av_frame_unref(frame_);
        }
    }
    av_packet_free(&packet);
    return decoded;
}

const QImage &H264Decoder::latestImage() const
{
    return latestImage_;
}

bool H264Decoder::ensureRgbFrame(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    const int pixelFormat = codecContext_->pix_fmt;
    if (!swsContext_ || currentWidth_ != width || currentHeight_ != height || currentPixFmt_ != pixelFormat)
    {
        sws_freeContext(swsContext_);
        swsContext_ = sws_getContext(width, height, codecContext_->pix_fmt,
                                     width, height, AV_PIX_FMT_RGB24,
                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!swsContext_)
        {
            return false;
        }
        currentWidth_ = width;
        currentHeight_ = height;
        currentPixFmt_ = pixelFormat;
    }

    if (rgbFrame_->data[0] && rgbFrame_->width == width &&
        rgbFrame_->height == height && rgbFrame_->format == AV_PIX_FMT_RGB24)
    {
        return true;
    }

    av_freep(&rgbFrame_->data[0]);
    if (av_image_alloc(rgbFrame_->data, rgbFrame_->linesize, width, height,
                       AV_PIX_FMT_RGB24, 1) < 0)
    {
        return false;
    }
    rgbFrame_->width = width;
    rgbFrame_->height = height;
    rgbFrame_->format = AV_PIX_FMT_RGB24;
    return true;
}
