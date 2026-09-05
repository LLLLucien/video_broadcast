#include "video_receiver.h"

#include "h264_decoder.h"
#include "params.h"
#include "rtp_packet.h"

#include <QHostAddress>
#include <QMutexLocker>
#include <QVector>
#include <QUdpSocket>

#include <iostream>


namespace
{
QByteArray withStartCode(const QByteArray &nal)
{
    return QByteArray("\0\0\0\1", 4) + nal;
}
}

VideoReceiver::VideoReceiver(QObject *parent)
    : QObject(parent), decoder_(new H264Decoder)
{
}

VideoReceiver::~VideoReceiver()
{
    delete decoder_;
}

QImage VideoReceiver::takeLatestFrame()
{
    QMutexLocker locker(&frameMutex_);
    QImage frame;
    frame.swap(latestFrame_);
    return frame;
}

void VideoReceiver::start()
{
    if (!decoder_->init())
    {
        emit statusChanged("解码器初始化失败");
        return;
    }

    socket_ = new QUdpSocket(this);
    connect(socket_, &QUdpSocket::readyRead, this, &VideoReceiver::onReadyRead);
    if (!socket_->bind(QHostAddress::AnyIPv4, vb::kUdpPort, QUdpSocket::ShareAddress))
    {
        emit statusChanged("端口 5004 被占用或无法绑定");
        return;
    }

    const QHostAddress multicastAddress(QString::fromUtf8(
        vb::kDefaultMulticastAddress.data(),
        static_cast<int>(vb::kDefaultMulticastAddress.size())));
    if (multicastAddress.isMulticast() && !socket_->joinMulticastGroup(multicastAddress))
    {
        std::cerr << "Failed to join multicast group "
                  << multicastAddress.toString().toStdString() << std::endl;
    }
    emit statusChanged("已监听 UDP 5004");
}

void VideoReceiver::onReadyRead()
{
    QVector<RtpPacket> packets;
    while (socket_->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(static_cast<int>(socket_->pendingDatagramSize()));
        if (socket_->readDatagram(datagram.data(), datagram.size()) <= 0)
        {
            continue;
        }

        RtpPacket packet;
        if (parseRtpPacket(datagram, packet) && packet.payloadType == vb::kPayloadTypeH264)
        {
            packets.push_back(std::move(packet));
        }
    }

    if (packets.isEmpty())
    {
        return;
    }

    const uint32_t newestTimestamp = packets.back().timestamp;
    for (const RtpPacket &packet : packets)
    {
        if (packet.timestamp != newestTimestamp)
        {
            continue;
        }

        if (hasAccessUnitTimestamp_ && packet.timestamp != accessUnitTimestamp_)
        {
            accessUnit_.clear();
            pendingNal_.clear();
            hasPendingSequence_ = false;
            hasAccessUnitTimestamp_ = false;
        }
        bool imageReady = false;
        accessUnitTimestamp_ = packet.timestamp;
        hasAccessUnitTimestamp_ = true;
        processNal(packet.payload, packet.seq);
        if (packet.marker)
        {
            imageReady = flushAccessUnit() || imageReady;
        }
        if (imageReady)
        {
            QMutexLocker locker(&frameMutex_);
            latestFrame_ = decoder_->latestImage();
        }
    }
}

void VideoReceiver::processNal(const QByteArray &nal, uint16_t sequence)
{
    if (nal.isEmpty())
    {
        return;
    }

    const uint8_t nalType = static_cast<uint8_t>(nal[0]) & 0x1F;
    if (nalType == 24)
    {
        size_t offset = 1;
        while (offset + 2 <= static_cast<size_t>(nal.size()))
        {
            const int index = static_cast<int>(offset);
            const uint16_t size = (static_cast<uint16_t>(static_cast<uint8_t>(nal[index])) << 8) |
                                  static_cast<uint16_t>(static_cast<uint8_t>(nal[index + 1]));
            offset += 2;
            if (offset + size > static_cast<size_t>(nal.size()))
            {
                return;
            }
            queueNal(nal.mid(static_cast<int>(offset), static_cast<int>(size)));
            offset += size;
        }
        return;
    }

    if (nalType == 28)
    {
        if (nal.size() < 2)
        {
            return;
        }
        const uint8_t indicator = static_cast<uint8_t>(nal[0]);
        const uint8_t header = static_cast<uint8_t>(nal[1]);
        if ((header & 0x80) != 0)
        {
            pendingNal_.clear();
            pendingNal_.append(static_cast<char>((indicator & 0xE0) | (header & 0x1F)));
            pendingNal_.append(nal.mid(2));
            pendingSequence_ = sequence;
            hasPendingSequence_ = true;
        }
        else if (!pendingNal_.isEmpty() && hasPendingSequence_ &&
                 static_cast<uint16_t>(pendingSequence_ + 1) == sequence)
        {
            pendingSequence_ = sequence;
            pendingNal_.append(nal.mid(2));
        }
        else
        {
            pendingNal_.clear();
            hasPendingSequence_ = false;
            return;
        }
        if ((header & 0x40) != 0 && !pendingNal_.isEmpty())
        {
            queueNal(pendingNal_);
            pendingNal_.clear();
            hasPendingSequence_ = false;
        }
        return;
    }
    queueNal(nal);
}

void VideoReceiver::queueNal(const QByteArray &nal)
{
    if (nal.isEmpty())
    {
        return;
    }
    const uint8_t nalType = static_cast<uint8_t>(nal[0]) & 0x1F;
    if (nalType == 7)
    {
        sps_ = nal;
        hasParameters_ = true;
    }
    else if (nalType == 8)
    {
        pps_ = nal;
        hasParameters_ = true;
    }
    else if (nalType == 5 && accessUnit_.isEmpty() && hasParameters_)
    {
        accessUnit_.append(withStartCode(sps_));
        accessUnit_.append(withStartCode(pps_));
    }
    accessUnit_.append(withStartCode(nal));
}

bool VideoReceiver::flushAccessUnit()
{
    bool frameReady = false;
    if (!accessUnit_.isEmpty())
    {
        frameReady = decoder_->decodeNAL(accessUnit_);
        accessUnit_.clear();
    }
    hasAccessUnitTimestamp_ = false;
    return frameReady;
}
