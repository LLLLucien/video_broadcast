#pragma once

#include <QMainWindow>
#include <QImage>

class QLabel;
class QThread;
class QTimer;
class VideoReceiver;

class ViewerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ViewerWindow(QWidget *parent = nullptr);
    ~ViewerWindow() override;

private slots:
    void updateImage();
    void updateStatus(const QString &message);

private:
    QLabel *label_ = nullptr;
    QThread *receiverThread_ = nullptr;
    VideoReceiver *receiver_ = nullptr;
    QTimer *displayTimer_ = nullptr;
};
