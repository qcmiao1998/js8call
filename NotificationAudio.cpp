#include "NotificationAudio.h"


NotificationAudio::NotificationAudio(QObject *parent) :
    QIODevice(parent),
    m_state(State::Stopped),
    m_input(&m_data),
    m_output(&m_data),
    m_decoder(nullptr),
    m_audio(nullptr)
{
    setOpenMode(QIODevice::ReadOnly);

    m_init = false;
    m_isDecodingFinished = false;
}

// initialize an audio device
void NotificationAudio::init(const QAudioDeviceInfo &device, const QAudioFormat& format) {
    m_device = device;
    m_format = format;

    if(!m_decoder){
        m_decoder = new QAudioDecoder(this);
        connect(m_decoder, &QAudioDecoder::bufferReady, this, &NotificationAudio::bufferReady);
        connect(m_decoder, &QAudioDecoder::finished, this, &NotificationAudio::finished);
    }
    m_decoder->setAudioFormat(m_format);

    if(!m_audio){
        m_audio = new QAudioOutput(m_device, m_format);
    }

    if (!m_output.open(QIODevice::ReadOnly) || !m_input.open(QIODevice::WriteOnly)){
        m_init = false;
        return;
    }

    m_init = true;
    emit initialized();
}

// play an audio file
void NotificationAudio::play(const QString &filePath) {
    QFile *file = new QFile(this);
    file->setFileName(filePath);
    if (!file->open(QIODevice::ReadOnly)){
        return;
    }

    playFile(file);
}

void NotificationAudio::playFile(QFile *file){
    if(!m_init || !m_decoder || !m_audio || !file){
        return;
    }

    resetBuffers();

    m_file = file;
    m_decoder->setSourceDevice(m_file);
    m_decoder->start();

    m_state = State::Playing;
    emit stateChanged(m_state);

    m_audio->start(this);
}

// Stop playing audio
void NotificationAudio::stop() {
    resetBuffers();
    m_state = State::Stopped;
    emit stateChanged(m_state);
}

// Reset the internal buffers and ensure the decoder and audio device is stopped
void NotificationAudio::resetBuffers() {
    if(m_audio){
        if(m_audio->state() != QAudio::StoppedState){
            m_audio->stop();
        }
    }

    if(m_decoder){
        if(m_decoder->state() != QAudioDecoder::StoppedState){
            m_decoder->stop();
        }
    }

    if(m_file){
        if(m_file->isOpen()){
            m_file->close();
        }
        delete m_file;
        m_file = nullptr;
    }

    m_data.clear();
    m_isDecodingFinished = false;
}

// io device, read into buffer.
qint64 NotificationAudio::readData(char* data, qint64 maxlen) {
    memset(data, 0, maxlen);

    if (m_state == State::Playing)
    {
        m_output.read(data, maxlen);

        // There is we send readed audio data via signal, for ability get audio signal for the who listen this signal.
        // Other word this emulate QAudioProbe behaviour for retrieve audio data which of sent to output device (speaker).
        if (maxlen > 0)
        {
            QByteArray buff(data, maxlen);
            emit newData(buff);
        }

        // Is finish of file
        if (atEnd())
        {
            stop();
        }
    }

    return maxlen;
}

// io device, unused.
qint64 NotificationAudio::writeData(const char* data, qint64 len) {
    Q_UNUSED(data);
    Q_UNUSED(len);

    return 0;
}

// io device, at end of device
bool NotificationAudio::atEnd() const {
    return m_output.size()
        && m_output.atEnd()
        && m_isDecodingFinished;
}

// handle buffered data ready
void NotificationAudio::bufferReady() {
    const QAudioBuffer &buffer = m_decoder->read();

    const int length = buffer.byteCount();
    const char *data = buffer.constData<char>();

    m_input.write(data, length);
}

// handle buffered data decoding is finished
void NotificationAudio::finished() {
    m_isDecodingFinished = true;
}
