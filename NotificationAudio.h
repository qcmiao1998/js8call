#ifndef NOTIFICATIONAUDIO_H
#define NOTIFICATIONAUDIO_H

#include <QIODevice>
#include <QBuffer>
#include <QAudioDecoder>
#include <QAudioDeviceInfo>
#include <QAudioFormat>
#include <QAudioOutput>
#include <QFile>

// Class for decode audio files like MP3 and push decoded audio data to QOutputDevice (like speaker) and also signal newData().
// For decoding it uses QAudioDecoder which uses QAudioFormat for decode audio file for desire format, then put decoded data to buffer.
// based on: https://github.com/Znurre/QtMixer
class NotificationAudio : public QIODevice
{
    Q_OBJECT

public:
    NotificationAudio(QObject * parent=nullptr);
    ~NotificationAudio();

    bool isInitialized() const { return m_init; }

    enum State { Playing, Stopped };

    bool atEnd() const override;

public slots:
    void init(const QAudioDeviceInfo &device, const QAudioFormat& format);
    void play(const QString &filePath);
    void stop();

protected:

    qint64 readData(char* data, qint64 maxlen) override;
    qint64 writeData(const char* data, qint64 len) override;

private:
    QFile *m_file;
    State m_state;
    QBuffer m_input;
    QBuffer m_output;
    QByteArray m_data;
    QAudioFormat m_format;
    QAudioDeviceInfo m_device;
    QAudioDecoder * m_decoder;
    QAudioOutput * m_audio;

    bool m_init;
    bool m_isDecodingFinished;

    void playFile(QFile *file);
    void resetBuffers();

private slots:
    void bufferReady();
    void finished();

signals:
    void initialized();
    void stateChanged(NotificationAudio::State state);
    void newData(const QByteArray& data);
};

#endif // NOTIFICATIONAUDIO_H
