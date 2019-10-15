#ifndef NOTIFICATIONAUDIO_H
#define NOTIFICATIONAUDIO_H

#include <QIODevice>
#include <QBuffer>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QFile>
#include <QPointer>

#include "AudioDevice.hpp"
#include "AudioDecoder.h"
#include "soundout.h"

class NotificationAudio :
    public QObject
{
    Q_OBJECT

public:
    NotificationAudio(QObject * parent=nullptr);
    ~NotificationAudio();

public slots:
    void setDevice(const QAudioDeviceInfo &device, unsigned channels, unsigned msBuffer=0);
    void play(const QString &filePath);
    void stop();

private:
    QPointer<SoundOutput> m_stream;
    QPointer<AudioDecoder> m_decoder;
};

#endif // NOTIFICATIONAUDIO_H
