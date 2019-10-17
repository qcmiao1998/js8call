#include "NotificationAudio.h"
#include "WaveFile.h"


NotificationAudio::NotificationAudio(QObject *parent):
    QObject(parent)
{
    m_stream = new SoundOutput();
    //m_decoder = new AudioDecoder(this);
    m_file = new WaveFile(this);
}

NotificationAudio::~NotificationAudio(){
    stop();
}

void NotificationAudio::setDevice(const QAudioDeviceInfo &device, unsigned channels, unsigned msBuffer){
    m_device = device;
    m_channels = channels;
    m_msBuffer = msBuffer;
    m_stream->setFormat(device, channels, msBuffer);
    //m_decoder->init(m_stream->format());
}

void NotificationAudio::play(const QString &filePath){
    //m_decoder->start(filePath);
    //m_stream->restart(m_decoder);
    if(m_file->isOpen()){
        m_file->close();
    }
    if(m_file->open(filePath)){
        m_file->seek(0);
        m_stream->setDeviceFormat(m_device, m_file->fileFormat(), m_channels, m_msBuffer);
        m_stream->restart(m_file);
    }
}

void NotificationAudio::stop(){
    //m_decoder->stop();
    m_stream->stop();

    if(m_file->isOpen()){
        m_file->close();
    }
}
