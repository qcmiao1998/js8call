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
#include "WaveFile.h"
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
    QPointer<WaveFile> m_file;
    QAudioDeviceInfo m_device;
    unsigned m_channels;
    unsigned m_msBuffer;
};

#endif // NOTIFICATIONAUDIO_H
