#include "APRSISClient.h"

#include <cmath>

#include "Radio.hpp"
#include "varicode.h"



APRSISClient::APRSISClient(QString host, quint16 port, QObject *parent):
    QTcpSocket(parent),
    m_host(host),
    m_port(port)
{
    connect(&m_timer, &QTimer::timeout, this, &APRSISClient::sendReports);
    m_timer.setInterval(60*1000); // every minute
    m_timer.start();
}

quint32 APRSISClient::hashCallsign(QString callsign){
    // based on: https://github.com/hessu/aprsc/blob/master/src/passcode.c
    QByteArray rootCall = QString(callsign.split("-").first().toUpper()).toLocal8Bit() + '\0';
    quint32 hash = 0x73E2;

    int i = 0;
    int len = rootCall.length();

    while(i+1 < len){
        hash ^= rootCall.at(i) << 8;
        hash ^= rootCall.at(i+1);
        i += 2;
    }

    return hash & 0x7FFF;
}

QString APRSISClient::loginFrame(QString callsign){
    auto loginFrame = QString("user %1 pass %2 ver %3\n");
    loginFrame = loginFrame.arg(callsign);
    loginFrame = loginFrame.arg(hashCallsign(callsign));
    loginFrame = loginFrame.arg("FT8Call");
    return loginFrame;
}

QList<QStringList> findall(QRegularExpression re, QString content){
    int pos = 0;

    QList<QStringList> all;
    while(pos < content.length()){
        auto match = re.match(content, pos);
        if(!match.hasMatch()){
            break;
        }

        all.append(match.capturedTexts());
        pos = match.capturedEnd();
    }

    return all;
}


inline long
floordiv (long num, long den)
{
  if (0 < (num^den))
    return num/den;
  else
    {
      ldiv_t res = ldiv(num,den);
      return (res.rem)? res.quot-1
                      : res.quot;
    }
}

// convert an arbitrary length grid locator to a high precision lat/lon
QPair<float, float> APRSISClient::grid2deg(QString locator){
    QString grid = locator.toUpper();

    float lat = -90;
    float lon = -90;

    auto lats = findall(QRegularExpression("([A-X])([A-X])"), grid);
    auto lons = findall(QRegularExpression("(\\d)(\\d)"), grid);

    int valx[22];
    int valy[22];

    int i = 0;
    int tot = 0;
    char A = 'A';
    foreach(QStringList matched, lats){
        char x = matched.at(1).at(0).toLatin1();
        char y = matched.at(2).at(0).toLatin1();

        valx[i*2] = x-A;
        valy[i*2] = y-A;

        i++;
        tot++;
    }

    i = 0;
    foreach(QStringList matched, lons){
        int x = matched.at(1).toInt();
        int y = matched.at(2).toInt();
        valx[i*2+1]=x;
        valy[i*2+1]=y;

        i++;
        tot++;
    }

    for(int i = 0; i < tot; i++){
        int x = valx[i];
        int y = valy[i];

        int z = i - 1;
        float scale = pow(10, floordiv(-(z-1), 2)) * pow(24, floordiv(-z, 2));

        lon += scale * x;
        lat += scale * y;
    }

    lon *= 2;

    return {lat, lon};
}

// convert an arbitrary length grid locator to a high precision lat/lon in aprs format
QPair<QString, QString> APRSISClient::grid2aprs(QString grid){
    auto geo = APRSISClient::grid2deg(grid);
    auto lat = geo.first;
    auto lon = geo.second;

    QString latDir = "N";
    if(lat < 0){
        lat *= -1;
        latDir = "S";
    }

    QString lonDir = "E";
    if(lon < 0){
        lon *= -1;
        lonDir = "W";
    }

    double iLat, fLat, iLon, fLon, iLatMin, fLatMin, iLonMin, fLonMin, iLatSec, iLonSec;
    fLat = modf(lat, &iLat);
    fLon = modf(lon, &iLon);

    fLatMin = modf(fLat * 60, &iLatMin);
    fLonMin = modf(fLon * 60, &iLonMin);

    iLatSec = round(fLatMin * 60);
    iLonSec = round(fLonMin * 60);

    if(iLatSec == 60){
        iLatMin += 1;
        iLatSec = 0;
    }

    if(iLonSec == 60){
        iLonMin += 1;
        iLonSec = 0;
    }

    if(iLatMin == 60){
        iLat += 1;
        iLatMin = 0;
    }

    if(iLonMin == 60){
        iLon += 1;
        iLonMin = 0;
    }

    double aprsLat = iLat * 100 + iLatMin + (iLatSec / 60.0);
    double aprsLon = iLon * 100 + iLonMin + (iLonSec / 60.0);

    return {
        QString().sprintf("%07.2f%%1", aprsLat).arg(latDir),
        QString().sprintf("%08.2f%%1", aprsLon).arg(lonDir)
    };
}

void APRSISClient::enqueueSpot(QString theircall, QString grid, quint64 frequency, int snr){
    if(m_localCall.isEmpty()) return;

    auto geo = APRSISClient::grid2aprs(grid);
    auto spotFrame = QString("%1>APRS,TCPIP*:=%2/%3nFT8CALL %4 %5 %6MHz %7dB\n");
    spotFrame = spotFrame.arg(theircall);
    spotFrame = spotFrame.arg(geo.first);
    spotFrame = spotFrame.arg(geo.second);
    spotFrame = spotFrame.arg(m_localCall);
    spotFrame = spotFrame.arg(m_localGrid.left(4));
    spotFrame = spotFrame.arg(Radio::frequency_MHz_string(frequency));
    spotFrame = spotFrame.arg(Varicode::formatSNR(snr));
    enqueueRaw(spotFrame);
}

void APRSISClient::enqueueMessage(QString tocall, QString message){
    if(m_localCall.isEmpty()) return;

    auto messageFrame = QString("%1>APRS,TCPIP*::%2:%3\n");
    messageFrame = messageFrame.arg(m_localCall);
    messageFrame = messageFrame.arg(tocall + QString(" ").repeated(9-tocall.length()));
    messageFrame = messageFrame.arg(message);
    enqueueRaw(messageFrame);
}

void APRSISClient::enqueueRaw(QString aprsFrame){
    m_frameQueue.enqueue(aprsFrame);
}

void APRSISClient::processQueue(bool disconnect){
    if(m_localCall.isEmpty()) return;

    // 1. connect (and read)
    // 2. login (and read)
    // 3. for each raw frame in queue, send
    // 4. disconnect

    if(state() != QTcpSocket::ConnectedState){
        connectToHost(m_host, m_port);
        if(!waitForConnected(5000)){
            qDebug() << "APRSISClient Connection Error:" << errorString();
            return;
        }
    }

    if(write(loginFrame(m_localCall).toLocal8Bit()) == -1){
        qDebug() << "APRSISClient Write Login Error:" << errorString();
        return;
    }

    while(!m_frameQueue.isEmpty()){
        if(write(m_frameQueue.head().toLocal8Bit()) == -1){
            qDebug() << "APRSISClient Write Error:" << errorString();
            return;
        }

        auto frame = m_frameQueue.dequeue();
        qDebug() << "APRISISClient Write:" << frame;
    }

    if(disconnect){
        disconnectFromHost();
    }
}
