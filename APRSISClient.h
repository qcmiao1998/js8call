#ifndef APRSISCLIENT_H
#define APRSISCLIENT_H

#include <QTcpSocket>
#include <QQueue>
#include <QPair>
#include <QTimer>

class APRSISClient : public QTcpSocket
{
public:
    APRSISClient(QString host, quint16 port, QObject *parent = nullptr);

    static quint32 hashCallsign(QString callsign);
    static QString loginFrame(QString callsign);
    static QPair<float, float> grid2deg(QString grid);
    static QPair<QString, QString> grid2aprs(QString grid);

    void setLocalStation(QString mycall, QString mygrid){
        m_localCall = mycall;
        m_localGrid = mygrid;
    }

    void enqueueSpot(QString theircall, QString grid, quint64 frequency, int snr);
    void enqueueMessage(QString tocall, QString message);
    void enqueueThirdParty(QString theircall, QString payload);
    void enqueueRaw(QString aprsFrame);

    void processQueue(bool disconnect=false);

public slots:
    void sendReports(){ processQueue(true); }

private:
    QString m_localCall;
    QString m_localGrid;

    QQueue<QString> m_frameQueue;
    QString m_host;
    quint16 m_port;
    QTimer m_timer;
};

#endif // APRSISCLIENT_H
