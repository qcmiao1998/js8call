#include "NotificationAudio.h"


NotificationAudio::NotificationAudio(QObject *parent):
    QObject(parent)
{
    m_stream = new SoundOutput();
    m_decoder = new AudioDecoder(this);
}

NotificationAudio::~NotificationAudio(){
    stop();
}

void NotificationAudio::setDevice(const QAudioDeviceInfo &device, unsigned channels, unsigned msBuffer){
    m_stream->setFormat(device, channels, msBuffer);
    m_decoder->init(m_stream->format());
}

void NotificationAudio::play(const QString &filePath){
    m_decoder->start(filePath);
    m_stream->restart(m_decoder);
}

void NotificationAudio::stop(){
    m_decoder->stop();
    m_stream->stop();
}
