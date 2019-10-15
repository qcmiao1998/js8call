#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include <QAudioDecoder>
#include <QAudioFormat>
#include <QBuffer>
#include <QIODevice>
#include <QPointer>


class AudioDecoder :
    public QIODevice
{
    Q_OBJECT
public:
    enum State { Decoding, Stopped };
    explicit AudioDecoder(QObject *parent = 0);
    ~AudioDecoder();

    bool atEnd() const override;

public slots:
    void init(const QAudioFormat &format);
    void start(const QString &filePath);
    void stop();

protected:
    qint64 readData(char* data, qint64 maxlen) override;
    qint64 writeData(const char* data, qint64 len) override;

private:
    State m_state;
    QPointer<QAudioDecoder> m_decoder;
    QBuffer m_input;
    QBuffer m_output;
    QByteArray m_data;
    bool m_init;
    bool m_isDecodingFinished;

private slots:
    void bufferReady();
    void finished();
    void errored(QAudioDecoder::Error);

signals:
    void initialized();
    void newData(const QByteArray& data);
};

#endif // AUDIODECODER_H
