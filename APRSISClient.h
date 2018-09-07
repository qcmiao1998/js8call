#ifndef APRSISCLIENT_H
#define APRSISCLIENT_H

#include <QtGlobal>
#include <QDateTime>
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

    void setServer(QString host, quint16 port){
        if(state() == QTcpSocket::ConnectedState){
            disconnectFromHost();
        }

        m_host = host;
        m_port = port;
    }

    void setPaused(bool paused){
        m_paused = paused;
    }

    void setLocalStation(QString mycall, QString mygrid, QString passcode){
        m_localCall = mycall;
        m_localGrid = mygrid;
        m_localPasscode = passcode;
    }

    bool isPasscodeValid(){ return m_localPasscode == QString::number(hashCallsign(m_localCall)); }

    void enqueueSpot(QString theircall, QString grid, QString comment);
    void enqueueThirdParty(QString theircall, QString payload);
    void enqueueRaw(QString aprsFrame);

    void processQueue(bool disconnect=true);

public slots:
    void sendReports(){
        if(m_paused) return;

        processQueue(true);
    }

private:
    QString m_localCall;
    QString m_localGrid;
    QString m_localPasscode;

    QQueue<QPair<QString, QDateTime>> m_frameQueue;
    QString m_host;
    quint16 m_port;
    QTimer m_timer;
    bool m_paused;
};

#endif // APRSISCLIENT_H
