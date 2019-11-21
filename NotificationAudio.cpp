#include "NotificationAudio.h"
#include "WaveFile.h"


NotificationAudio::NotificationAudio(QObject *parent):
    QObject(parent)
{
    m_file = new BWFFile(QAudioFormat{}, this);

    m_stream = new SoundOutput();

    connect(m_stream, &SoundOutput::status, this, &NotificationAudio::status);
    connect(m_stream, &SoundOutput::error, this, &NotificationAudio::error);
}

NotificationAudio::~NotificationAudio(){
    stop();
}

void NotificationAudio::status(QString message){
    if(message == "Idle"){
        stop();
    }
}

void NotificationAudio::error(QString message){
    qDebug() << "notification error:" << message;
    stop();
}

void NotificationAudio::setDevice(const QAudioDeviceInfo &device, unsigned channels, unsigned msBuffer){
    m_device = device;
    m_channels = channels;
    m_msBuffer = msBuffer;
    m_stream->setFormat(device, channels, msBuffer);
}

void NotificationAudio::play(const QString &filePath){
    if(m_file->isOpen()){
        m_file->close();
    }
    m_file->setFileName(filePath);
    if(m_file->open(BWFFile::ReadOnly)){
        auto format = m_file->format();
        auto channels = format.channelCount();
        auto bytes = m_file->readAll();
        auto buffer = new QBuffer(new QByteArray(bytes));
        if(buffer->open(QIODevice::ReadOnly)){
            m_stream->setDeviceFormat(m_device, format, channels, m_msBuffer);
            m_stream->restart(buffer);
        }
    }
}

void NotificationAudio::stop(){
    m_stream->stop();

    if(m_file->isOpen()){
        m_file->close();
    }
}
