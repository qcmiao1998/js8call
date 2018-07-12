#ifndef VARICODE_H
#define VARICODE_H

/**
 * (C) 2018 Jordan Sherer <kn4crd@gmail.com> - All Rights Reserved
 **/

#include <QBitArray>
#include <QString>
#include <QVector>

class Varicode
{
public:
    //Varicode();

    QVector<bool> intToBits(qint64 value, int expected=0);
    qint64 bitsToInt(QVector<bool> const value);

    qint8 unpack5bits(QString const& value);
    QString pack5bits(qint8 packed);

    qint16 unpack16bits(QString const& value);
    QString pack16bits(qint16 packed);

    qint32 unpack32bits(QString const& value);
    QString pack32bits(qint32 packed);

    qint64 unpack64bits(QString const& value);
    QString pack64bits(qint64 packed);
};

#endif // VARICODE_H
