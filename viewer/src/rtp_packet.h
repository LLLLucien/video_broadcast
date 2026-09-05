#pragma once

#include <QByteArray>
#include <cstdint>

struct RtpPacket
{
    uint8_t version = 0;
    bool marker = false;
    uint8_t payloadType = 0;
    uint16_t seq = 0;
    uint32_t timestamp = 0;
    uint32_t ssrc = 0;
    QByteArray payload;
};

bool parseRtpPacket(const QByteArray &datagram, RtpPacket &packet);
