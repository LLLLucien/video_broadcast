#include "viewer_window.h"

#include "params.h"
#include "video_receiver.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QWidget>

ViewerWindow::ViewerWindow(QWidget *parent)
    : QMainWindow(parent)
{
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    label_ = new QLabel(this);
    label_->setMinimumSize(vb::kWidth, vb::kHeight);
    label_->setAlignment(Qt::AlignCenter);
    label_->setStyleSheet("QLabel { background: black; color: white; }");
    label_->setText("等待信号...");
    layout->addWidget(label_);
    setCentralWidget(central);
    resize(vb::kWidth + 40, vb::kHeight + 60);
    setWindowTitle("Viewer");

    receiverThread_ = new QThread(this);
    receiver_ = new VideoReceiver;
    receiver_->moveToThread(receiverThread_);
    connect(receiverThread_, &QThread::started, receiver_, &VideoReceiver::start);
    connect(receiver_, &VideoReceiver::statusChanged, this, &ViewerWindow::updateStatus);
    receiverThread_->start();

    displayTimer_ = new QTimer(this);
    displayTimer_->setInterval(33);
    connect(displayTimer_, &QTimer::timeout, this, &ViewerWindow::updateImage);
    displayTimer_->start();
}

ViewerWindow::~ViewerWindow()
{
    receiverThread_->quit();
    receiverThread_->wait();
    delete receiver_;
}

void ViewerWindow::updateImage()
{
    const QImage image = receiver_->takeLatestFrame();
    if (image.isNull())
    {
        // takeLatestFrame() clears latestImage_; a null image means no new
        // decoded frame has arrived since the previous poll. Skipping here
        // avoids redundant per-tick repaints and allocations.
        return;
    }

    // Decoded frames already arrive at the native video resolution. Setting
    // the pixmap directly (and letting QLabel scale via setScaledContents)
    // avoids a redundant full-frame software scale + allocation every tick,
    // removing a sustained per-frame heap-allocation source that can fragment
    // memory and cause gradually worsening jank over a long run.
    label_->setScaledContents(true);
    label_->setPixmap(QPixmap::fromImage(image));
    label_->setText(QString());
}

void ViewerWindow::updateStatus(const QString &message)
{
    statusBar()->showMessage(message);
}
