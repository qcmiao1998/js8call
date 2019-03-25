#ifndef JS8SPOTCLIENT_H
#define JS8SPOTCLIENT_H

#include "MessageClient.hpp"

#include <QObject>
#include <QHostInfo>
#include <QQueue>
#include <QTimer>

class SpotClient : public QObject
{
    Q_OBJECT
public:
    SpotClient(MessageClient *client, QObject *parent = nullptr);

    void prepare();
    void setLocalStation(QString callsign, QString grid, QString info, QString version);
    void enqueueLocalSpot(QString callsign, QString grid, QString info, QString version);
    void enqueueSpot(QString callsign, QString grid, int frequency, int snr);
    void sendRawSpot(QByteArray payload);

public slots:
    void processSpots();
    void dnsLookupResult(QHostInfo);

private:
    QString m_call;
    QString m_grid;
    QString m_info;
    QString m_version;

    QHostAddress m_address;
    MessageClient *m_client;
    QTimer m_timer;
    QQueue<QByteArray> m_queue;
};

#endif // JS8SPOTCLIENT_H
