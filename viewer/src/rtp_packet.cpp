#include "rtp_packet.h"
#include "params.h"

bool parseRtpPacket(const QByteArray &datagram, RtpPacket &packet)
{
    if (datagram.size() < vb::kRtpHeaderSize)
    {
        return false;
    }

    const auto *data = reinterpret_cast<const uint8_t *>(datagram.constData());
    packet.version = (data[0] >> 6) & 0x03;
    if (packet.version != 2)
    {
        return false;
    }

    const bool hasPadding = (data[0] & 0x20) != 0;
    const bool hasExtension = (data[0] & 0x10) != 0;
    const int csrcCount = data[0] & 0x0F;
    int headerSize = vb::kRtpHeaderSize + csrcCount * 4;
    if (headerSize > datagram.size())
    {
        return false;
    }

    if (hasExtension)
    {
        if (headerSize + 4 > datagram.size())
        {
            return false;
        }
        const int extensionWords = (static_cast<int>(data[headerSize + 2]) << 8) |
                                    static_cast<int>(data[headerSize + 3]);
        headerSize += 4 + extensionWords * 4;
        if (headerSize > datagram.size())
        {
            return false;
        }
    }

    int payloadSize = datagram.size() - headerSize;
    if (hasPadding)
    {
        const uint8_t paddingSize = data[datagram.size() - 1];
        if (paddingSize == 0 || paddingSize > payloadSize)
        {
            return false;
        }
        payloadSize -= paddingSize;
    }

    packet.marker = (data[1] & 0x80) != 0;
    packet.payloadType = data[1] & 0x7F;
    packet.seq = static_cast<uint16_t>((data[2] << 8) | data[3]);
    packet.timestamp = (static_cast<uint32_t>(data[4]) << 24) |
                       (static_cast<uint32_t>(data[5]) << 16) |
                       (static_cast<uint32_t>(data[6]) << 8) |
                       static_cast<uint32_t>(data[7]);
    packet.ssrc = (static_cast<uint32_t>(data[8]) << 24) |
                  (static_cast<uint32_t>(data[9]) << 16) |
                  (static_cast<uint32_t>(data[10]) << 8) |
                  static_cast<uint32_t>(data[11]);
    packet.payload = datagram.mid(headerSize, payloadSize);
    return !packet.payload.isEmpty();
}
