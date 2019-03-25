#include "SpotClient.h"
#include "Message.h"

#include "moc_SpotClient.cpp"

SpotClient::SpotClient(MessageClient *client, QObject *parent):
    QObject(parent),
    m_client { client }
{
    prepare();

    connect(&m_timer, &QTimer::timeout, this, &SpotClient::processSpots);
    m_timer.setInterval(60 * 1000);
    m_timer.setSingleShot(false);
    m_timer.start();
}

void SpotClient::prepare(){
    QHostInfo::lookupHost("spot.js8call.com", this, SLOT(dnsLookupResult(QHostInfo)));
}

void SpotClient::dnsLookupResult(QHostInfo info){
    if (info.addresses().isEmpty()) {
        qDebug() << "SpotClient Error:" << info.errorString();
        return;
    }

    m_address = info.addresses().at(0);
    qDebug() << "SpotClient Resolve:" << m_address.toString();
}

void SpotClient::setLocalStation(QString callsign, QString grid, QString info, QString version){
    bool changed = false;

    if(m_call != callsign){
        m_call = callsign;
        changed = true;
    }

    if(m_grid != grid){
        m_grid = grid;
        changed = true;
    }

    if(m_info != info){
        m_info = info;
        changed = true;
    }

    if(m_version != version){
        m_version = version;
        changed = true;
    }

    // send local information to network on change, or once every 5 minutes
    if(changed || m_seq % 5 == 0){
        enqueueLocalSpot(callsign, grid, info, version);
    }
}

void SpotClient::enqueueLocalSpot(QString callsign, QString grid, QString info, QString version){
    auto m = Message("RX.LOCAL", "", {
        {"CALLSIGN", QVariant(callsign)},
        {"GRID", QVariant(grid)},
        {"INFO", QVariant(info)},
        {"VERSION", QVariant(version)},
    });

    m_queue.enqueue(m.toJson());
}

void SpotClient::enqueueSpot(QString callsign, QString grid, int frequency, int snr){
    auto m = Message("RX.SPOT", "", {
         {"BY", QVariant(m_call)},
         {"CALLSIGN", QVariant(callsign)},
         {"GRID", QVariant(grid)},
         {"FREQ", QVariant(frequency)},
         {"SNR", QVariant(snr)},
    });

    // queue these...
    m_queue.enqueue(m.toJson());
}

void SpotClient::processSpots(){
    if(m_address.isNull()){
        prepare();
        return;
    }

    while(!m_queue.isEmpty()){
        sendRawSpot(m_queue.dequeue());
    }

    m_seq++;
}

void SpotClient::sendRawSpot(QByteArray payload){
    if(!m_address.isNull()){
        m_client->send_raw_datagram(payload, m_address, 50000);
    }
}
