#pragma once

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QMutex>

#include <cstdint>

class H264Decoder;
class QUdpSocket;

class VideoReceiver : public QObject
{
    Q_OBJECT

public:
    explicit VideoReceiver(QObject *parent = nullptr);
    ~VideoReceiver() override;

    QImage takeLatestFrame();

public slots:
    void start();

signals:
    void statusChanged(const QString &message);

private slots:
    void onReadyRead();

private:
    void processNal(const QByteArray &nal, uint16_t sequence);
    void queueNal(const QByteArray &nal);
    bool flushAccessUnit();

    QUdpSocket *socket_ = nullptr;
    H264Decoder *decoder_ = nullptr;
    QByteArray pendingNal_;
    uint16_t pendingSequence_ = 0;
    bool hasPendingSequence_ = false;
    QByteArray accessUnit_;
    uint32_t accessUnitTimestamp_ = 0;
    bool hasAccessUnitTimestamp_ = false;
    QByteArray sps_;
    QByteArray pps_;
    bool hasParameters_ = false;
    QMutex frameMutex_;
    QImage latestFrame_;
};
